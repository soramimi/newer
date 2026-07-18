
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>

#define O_BINARY 0

bool copy(int fd1, int fd2)
{
	ftruncate(fd2, 0);
	char buffer[4096];
	ssize_t bytes_read;
	while ((bytes_read = read(fd1, buffer, sizeof(buffer))) > 0) {
		ssize_t bytes_written = 0;
		while (bytes_written < bytes_read) {
			ssize_t result = write(fd2, buffer + bytes_written, bytes_read - bytes_written);
			if (result < 0) {
				return false;
			}
			bytes_written += result;
		}
	}
	if (bytes_read < 0) {
		return false;
	}
	return true;
	
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		return 1;
	}
	char const *left = argv[1];
	char const *right = argv[2];
	
	int fd1 = open(left, O_RDWR);
	int fd2 = open(right, O_RDWR);
	if (fd1 < 0 && fd2 < 0) {
		return 1;
	}

	struct stat st1;
	if (fstat(fd1, &st1) < 0) {
		close(fd1);
		fd1 = open(left, O_RDWR | O_CREAT | O_TRUNC | O_BINARY);
		if (fd1 < 0) {
			return 1;
		}
		st1 = {};
	}
	
	struct stat st2;
	if (fstat(fd2, &st2) < 0) {
		close(fd2);
		fd2 = open(right, O_RDWR | O_CREAT | O_TRUNC | O_BINARY);
		if (fd2 < 0) {
			return 1;
		}
		st2 = {};
	}
	
	if (fd1 < 0) {
	}
	
	if (fd2 < 0) {
	}
	
	struct utimbuf time;
	if (st1.st_mtime > st2.st_mtime) {
		copy(fd1, fd2);
		time.actime = st1.st_atim.tv_sec;
		time.modtime = st1.st_mtim.tv_sec;
		close(fd1);
		close(fd2);
		chmod(right, st1.st_mode);
		utime(right, &time);
	} else if (st1.st_mtime < st2.st_mtime) {
		copy(fd2, fd1);
		time.actime = st2.st_atim.tv_sec;
		time.modtime = st2.st_mtim.tv_sec;
		close(fd1);
		close(fd2);
		chmod(left, st2.st_mode);
		utime(left, &time);
	} else {
		close(fd1);
		close(fd2);
	}
	
	return 0;
}
