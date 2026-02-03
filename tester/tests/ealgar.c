/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ealgar.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vzurera- <vzurera-@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/26 20:12:11 by ealgar-c          #+#    #+#             */
/*   Updated: 2026/02/03 21:48:42 by vzurera-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma region "Includes"

	#include <stdlib.h>
	#include <string.h>
	#include <unistd.h>
	#include <stdarg.h>

#pragma endregion

#pragma region "Defines"

	#define GREEN   "\033[0;32m"
	#define BLUE    "\033[0;34m"
	#define YELLOW  "\033[0;33m"
	#define RESET   "\033[0m"
	#define M_CHECK_ACTION -5

	int mallopt(int param, int value);
	void show_alloc_mem(void);

#pragma endregion

#pragma region "Printf"

	#pragma region "Char"

		static void	print_c(int c, int *total, int fd) {
			int	result;

			result = write(fd, &c, 1);
			*total += result;
			if (result == -1) *total = -1;
		}

	#pragma endregion

	#pragma region "String"

		static void	print_s(char *s, int *total, int fd) {
			if (!s) s = "(null)";
			while (*s && *total >= 0) print_c(*s++, total, fd);
		}

	#pragma endregion

	#pragma region "Number"

		static void	print_n(unsigned long n, unsigned int base, int *total, int fd) {
			char	*strbase = "0123456789abcdef";

			if (base == 11) {		base = 10; print_c('-', total, fd);  }
			else if (base == 17) {	base = 16; print_s("0x", total, fd); }
			if (fd < 0)				strbase = "0123456789ABCDEF";
			if (n >= base)			print_n(n / base, base, total, fd);
			if (*total == -1)		return;
			if (fd < 0)				fd = -fd;

			print_c(strbase[n % base], total, fd);
		}

	#pragma endregion

	#pragma region "Chooser"

		static void	chooser(int c, va_list args, int *total, int fd) {
			if (c == '%')	print_c('%', total, fd);
			if (c == 'c')	print_c(va_arg(args, int), total, fd);
			if (c == 's')	print_s(va_arg(args, char *), total, fd);
			if (c == 'u')	print_n(va_arg(args, unsigned int), 10, total, fd);
			if (c == 'p')	print_n(va_arg(args, unsigned long), 17, total, fd);
			if (c == 'x')	print_n(va_arg(args, unsigned int), 16, total, fd);
			if (c == 'X')	print_n(va_arg(args, unsigned int), 16, total, -fd);
			if (c == 'd' || c == 'i') { long long n = va_arg(args, int);
				if (n < 0)	print_n(n * -1, 11, total, fd);
				else		print_n(n, 10, total, fd);
			}
		}

	#pragma endregion

	#pragma region "Printf"

		int	ft_printf(int fd, char const *format, ...) {
			va_list	args; va_start(args, format);
			int		total = 0;

			if (fd < 1) return (0);

			while (format && *format && total >= 0) {
				if (*format == '%')	chooser(*(++format), args, &total, fd);
				else				print_c(*format, &total, fd);
				format++;
			}

			return (va_end(args), total);
		}

	#pragma endregion

#pragma endregion

#pragma region "Tests"

	#pragma region "Test 00"

		int test_00(void) {
			char *str = malloc(12);
			if (!str)
				return -1;
			ft_printf(1, BLUE "\tAllocated 12 bytes string at %p\n" RESET, str);
			strcpy(str, "Hello world");
			ft_printf(1, BLUE "\tContents at %p: %s\n" RESET, str, str);
			show_alloc_mem();
			free(str);
			ft_printf(1, BLUE "\tFreed memory at %p\n" RESET, str);
			show_alloc_mem();
			return 0;
		}

	#pragma endregion

	#pragma region "Test 01"

		int test_01(void) {
			int *arr = malloc(210 * sizeof(int));
			if (!arr)
				return -1;
			ft_printf(1, BLUE "\tAllocated int array at %p (size: %d bytes)\n" RESET, arr, 210 * (int)sizeof(int));
			for (int i = 0; i < 210; i++)
				arr[i] = i * i;
			ft_printf(1, BLUE "\tContents at %p: [%d, %d, %d, ...]\n" RESET, arr, arr[0], arr[1], arr[2]);
			show_alloc_mem();
			free(arr);
			ft_printf(1, BLUE "\tFreed memory at %p\n" RESET, arr);
			show_alloc_mem();
			return 0;
		}

	#pragma endregion

	#pragma region "Test 02"

		int test_02(void) {
			char *big_str = malloc(1024 * 1024);
			if (!big_str)
				return -1;
			ft_printf(1, BLUE "\tAllocated 1 MB block at %p\n" RESET, big_str);
			memset(big_str, 'A', 1024 * 1024);
			ft_printf(1, BLUE "\tFirst 16 bytes at %p: ", big_str);
			for (int i = 0; i < 16; i++)
				ft_printf(1, "%c", big_str[i]);
			ft_printf(1, "\n" RESET);
			show_alloc_mem();
			free(big_str);
			ft_printf(1, BLUE "\tFreed memory at %p\n" RESET, big_str);
			show_alloc_mem();
			return 0;
		}

	#pragma endregion

	#pragma region "Test 03"

		int test_03(void) {
			char *str = malloc(6);
			if (!str)
				return -1;
			strcpy(str, "Hello");
			ft_printf(1, BLUE "\tAllocated 6 bytes string at %p\n" RESET, str);
			ft_printf(1, BLUE "\tContents at %p: %s\n" RESET, str, str);
			show_alloc_mem();
			char *new_str = realloc(str, 12);
			if (!new_str)
				return -1;
			strcat(new_str, " world");
			ft_printf(1, BLUE "\tReallocated to 12 bytes at %p\n" RESET, new_str);
			ft_printf(1, BLUE "\tContents at %p: %s\n" RESET, new_str, new_str);
			show_alloc_mem();
			free(new_str);
			ft_printf(1, BLUE "\tFreed memory at %p\n" RESET, new_str);
			show_alloc_mem();
			return 0;
		}

	#pragma endregion

	#pragma region "Test 04"

		int test_04(void) {
			int *data = malloc(20 * sizeof(int));
			if (!data)
				return -1;
			ft_printf(1, BLUE "\tAllocated int array at %p (size: %d bytes)\n" RESET, data, 20 * (int)sizeof(int));
			for (int i = 0; i < 20; i++)
				data[i] = i;
			ft_printf(1, BLUE "\tContents at %p: [%d, %d, %d, ...]\n" RESET, data, data[0], data[1], data[2]);
			show_alloc_mem();
			free(data);
			ft_printf(1, BLUE "\tFreed memory at %p\n" RESET, data);
			show_alloc_mem();
			return 0;
		}

	#pragma endregion

	#pragma region "Test 05"

		int test_05(void) {
			void *ptrs[15];
			int sizes[15] = {8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768,65536,131072};

			ft_printf(1, BLUE "\tAllocating 15 blocks of increasing sizes...\n" RESET);
			for (int i = 0; i < 15; i++) {
				ptrs[i] = malloc(sizes[i]);
				if (!ptrs[i])
					return -1;
				memset(ptrs[i], 'A' + (i % 26), sizes[i]);
				ft_printf(1, BLUE "\tBlock %d: %d bytes at %p, first 8 bytes: ", i, sizes[i], ptrs[i]);
				for (int j = 0; j < 8; j++)
					ft_printf(1, "%c", ((char *)ptrs[i])[j]);
				ft_printf(1, "\n" RESET);
			}
			show_alloc_mem();

			ft_printf(1, BLUE "\tFreeing some blocks (3,7,11)...\n" RESET);
			// Esto produce double free y invalid pointer porque se liberan aquí y en el último bloque for
			// El invalid pointer es porque ptrs[11] crea una zona LARGE que luego se elimina
			free(ptrs[3]); ft_printf(1, BLUE "\tFreed memory at %p\n" RESET, ptrs[3]);
			free(ptrs[7]); ft_printf(1, BLUE "\tFreed memory at %p\n" RESET, ptrs[7]);
			free(ptrs[11]); ft_printf(1, BLUE "\tFreed memory at %p\n" RESET, ptrs[11]);
			show_alloc_mem();

			ft_printf(1, BLUE "\tReallocating block 4 from %d to %d bytes\n" RESET, sizes[4], sizes[4] * 2);
			ptrs[4] = realloc(ptrs[4], sizes[4] * 2);
			if (!ptrs[4])
				return -1;
			memset(ptrs[4], '*', sizes[4] * 2);
			ft_printf(1, BLUE "\tBlock 4 after realloc: %d bytes at %p, first 8 bytes: ", sizes[4] * 2, ptrs[4]);
			for (int j = 0; j < 8; j++)
				ft_printf(1, "%c", ((char *)ptrs[4])[j]);
			ft_printf(1, "\n" RESET);
			show_alloc_mem();

			ft_printf(1, BLUE "\tFreeing all blocks...\n" RESET);
			for (int i = 0; i < 15; i++)
				if (ptrs[i]) {
					free(ptrs[i]);
					ft_printf(1, BLUE "\tFreed memory at %p\n" RESET, ptrs[i]);
				}
			show_alloc_mem();
			return 0;
		}

	#pragma endregion

	#pragma region "Test 150 Tiny"

		int test_150_tiny(void) {
			void *ptrs[150];

			ft_printf(1, BLUE "\tAllocating 150 TINY blocks...\n" RESET);
			for (int i = 0; i < 150; i++) {
				ptrs[i] = malloc(200);
				if (!ptrs[i])
					return -1;
				memset(ptrs[i], 'a' + (i % 26), 200);
				if (i < 3 || i >= 147) {
					ft_printf(1, BLUE "\tBlock %d at %p, first 8 bytes: ", i, ptrs[i]);
					for (int j = 0; j < 8; j++)
						ft_printf(1, "%c", ((char *)ptrs[i])[j]);
					ft_printf(1, "\n" RESET);
				}
			}
			show_alloc_mem();

			ft_printf(1, BLUE "\tFreeing all 150 blocks...\n" RESET);
			for (int i = 0; i < 150; i++) {
				free(ptrs[i]);
				if (i < 3 || i >= 147)
					ft_printf(1, BLUE "\tFreed memory at %p\n" RESET, ptrs[i]);
			}
			show_alloc_mem();
			return 0;
		}

	#pragma endregion

#pragma endregion

#pragma region "Main"

int main(void) {

	mallopt(M_CHECK_ACTION, 1); // Para evitar abort() en el test_05

	ft_printf(1, GREEN "\t\tSTARTING MALLOC TESTS:\n" RESET);

	ft_printf(1, YELLOW "\t\t00 - Hello world\n" RESET);
	test_00();

	ft_printf(1, YELLOW "\n\t\t01 - Int array allocation\n" RESET);
	test_01();

	ft_printf(1, YELLOW "\n\t\t02 - Large allocation (1 MB)\n" RESET);
	test_02();

	ft_printf(1, YELLOW "\n\t\t03 - Realloc growing string\n" RESET);
	test_03();

	ft_printf(1, YELLOW "\n\t\t04 - Allocate and free int array\n" RESET);
	test_04();

	ft_printf(1, YELLOW "\n\t\t05 - 15 mallocs, realloc and frees\n" RESET);
	test_05();

	ft_printf(1, YELLOW "\n\t\tFINAL - test_150_tiny_mallocs\n" RESET);
	test_150_tiny();

	return 0;
}

#pragma endregion
