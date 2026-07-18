
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#define O_BINARY 0

class FileGuard {
	int fd_;

public:
	explicit FileGuard(int fd = -1)
		: fd_(fd)
	{
	}
	~FileGuard() { close(); }
	FileGuard(FileGuard const &) = delete;
	FileGuard &operator=(FileGuard const &) = delete;
	FileGuard(FileGuard &&other) noexcept
		: fd_(other.fd_)
	{
		other.fd_ = -1;
	}
	FileGuard &operator=(FileGuard &&other) noexcept
	{
		if (this != &other) {
			close();
			fd_ = other.fd_;
			other.fd_ = -1;
		}
		return *this;
	}
	void close()
	{
		if (fd_ >= 0) {
			::close(fd_);
			fd_ = -1;
		}
	}
	int get() const { return fd_; }
	int release()
	{
		int t = fd_;
		fd_ = -1;
		return t;
	}
	bool valid() const { return fd_ >= 0; }
};

static bool copy_content(int fd_src, int fd_dst)
{
	constexpr size_t BUF_SIZE = 65536;
	std::vector<char> buffer(BUF_SIZE);
	if (lseek(fd_src, 0, SEEK_SET) < 0) return false;
	if (lseek(fd_dst, 0, SEEK_SET) < 0) return false;
	if (ftruncate(fd_dst, 0) < 0) return false;
	ssize_t n;
	while ((n = read(fd_src, buffer.data(), buffer.size())) > 0) {
		char *p = buffer.data();
		ssize_t w = 0;
		while (w < n) {
			ssize_t r = write(fd_dst, p + w, n - w);
			if (r < 0) return false;
			w += r;
		}
	}
	return n == 0;
}

static bool copy_metadata(int fd, struct stat const &st, bool verbose)
{
	struct timespec ts[2];
	ts[0] = st.st_atim;
	ts[1] = st.st_mtim;
	if (futimens(fd, ts) < 0) {
		perror("futimens");
		return false;
	}
	if (fchmod(fd, st.st_mode) < 0) {
		perror("fchmod");
		return false;
	}
	if (fchown(fd, st.st_uid, st.st_gid) < 0 && verbose) {
		fprintf(stderr, "warning: fchown: %s\n", strerror(errno));
	}
	return true;
}

static bool atomic_copy(const char *dst_path, int fd_src, struct stat const &st_src, bool verbose)
{
	std::string dst_dir(dst_path);
	char *dir = ::dirname(dst_dir.data());
	char template_path[PATH_MAX];
	int n = snprintf(template_path, sizeof(template_path), "%s/.newer_XXXXXX", dir);
	if (n < 0 || static_cast<size_t>(n) >= sizeof(template_path)) {
		fprintf(stderr, "path too long\n");
		return false;
	}
	int fd_tmp = mkstemp(template_path);
	if (fd_tmp < 0) {
		perror("mkstemp");
		return false;
	}
	FileGuard guard_tmp(fd_tmp);
	if (!copy_content(fd_src, fd_tmp)) {
		perror("copy_content");
		unlink(template_path);
		return false;
	}
	if (!copy_metadata(fd_tmp, st_src, verbose)) {
		unlink(template_path);
		return false;
	}
	guard_tmp.close();
	if (rename(template_path, dst_path) < 0) {
		perror("rename");
		unlink(template_path);
		return false;
	}
	return true;
}

static bool do_copy(const char *dst_path, int fd_src, struct stat const &st_src, bool atomic, bool verbose)
{
	if (atomic) {
		return atomic_copy(dst_path, fd_src, st_src, verbose);
	}
	int fd_dst = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	if (fd_dst < 0) {
		perror("open");
		return false;
	}
	FileGuard guard_dst(fd_dst);
	if (!copy_content(fd_src, fd_dst)) {
		perror("copy_content");
		return false;
	}
	if (!copy_metadata(fd_dst, st_src, verbose)) {
		return false;
	}
	return true;
}

int main(int argc, char **argv)
{
	bool verbose = false;
	bool dry_run = false;
	bool atomic = false;

	std::vector<char const *> positional;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
			verbose = true;
		} else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--dry-run") == 0) {
			dry_run = true;
		} else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--atomic") == 0) {
			atomic = true;
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printf("usage: %s [options] <left> <right>\n"
				   "Options:\n"
				   "  -v, --verbose    Print detailed progress\n"
				   "  -n, --dry-run    Show what would happen without copying\n"
				   "  -a, --atomic     Use atomic write (write to temp file, then rename)\n"
				   "  -h, --help       Show this help message\n",
				argv[0]);
			return 0;
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "unknown option: %s\n", argv[i]);
			return 1;
		} else {
			positional.push_back(argv[i]);
		}
	}

	if (positional.size() != 2) {
		fprintf(stderr, "usage: %s [options] <left> <right>\n", argv[0]);
		return 1;
	}

	char const *left = positional[0];
	char const *right = positional[1];

	struct stat st1, st2;
	bool exists1 = (stat(left, &st1) == 0);
	bool exists2 = (stat(right, &st2) == 0);

	if (!exists1 && !exists2) {
		fprintf(stderr, "error: both files do not exist\n");
		return 1;
	}

	if (exists1 && exists2 && st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino) {
		fprintf(stderr, "error: same file\n");
		return 1;
	}

	char const *src_path = nullptr;
	char const *dst_path = nullptr;
	struct stat *src_stat = nullptr;

	if (!exists1) {
		src_path = right;
		src_stat = &st2;
		dst_path = left;
		if (verbose || dry_run) printf("%s -> %s (left missing)\n", right, left);
	} else if (!exists2) {
		src_path = left;
		src_stat = &st1;
		dst_path = right;
		if (verbose || dry_run) printf("%s -> %s (right missing)\n", left, right);
	} else if (st1.st_mtime > st2.st_mtime) {
		src_path = left;
		src_stat = &st1;
		dst_path = right;
		if (verbose || dry_run) printf("%s -> %s (newer)\n", left, right);
	} else if (st1.st_mtime < st2.st_mtime) {
		src_path = right;
		src_stat = &st2;
		dst_path = left;
		if (verbose || dry_run) printf("%s -> %s (newer)\n", right, left);
	} else {
		if (verbose) printf("same timestamp, nothing to do\n");
		return 0;
	}

	if (dry_run) {
		return 0;
	}

	int fd_src = open(src_path, O_RDONLY | O_BINARY);
	if (fd_src < 0) {
		perror("open");
		return 1;
	}
	FileGuard guard_src(fd_src);

	bool ok = do_copy(dst_path, fd_src, *src_stat, atomic, verbose);
	return ok ? 0 : 1;
}
