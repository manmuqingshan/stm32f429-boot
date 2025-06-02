#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

__attribute__((weak)) int _close(int fd) {
	if (fd >= STDIN_FILENO && fd <= STDERR_FILENO) return 0;
		errno = EBADF;
	return -1;
}

__attribute__((weak)) int _read(int file, char *ptr, int len) {
	(void)file;
	for (int DataIdx = 0; DataIdx < len; DataIdx++) {
		//*ptr++ = __io_getchar();
	}
	return len;
}

__attribute__((weak)) int _write(int file, char *ptr, int len) {
	(void)file;
	for (int DataIdx = 0; DataIdx < len; DataIdx++) {
		//__io_putchar(*ptr++);
	}
	return len;
}