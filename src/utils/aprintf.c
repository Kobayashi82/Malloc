/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   aprintf.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vzurera- <vzurera-@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/01/31 23:43:13 by vzurera-          #+#    #+#             */
/*   Updated: 2025/07/05 18:19:10 by vzurera-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma region "Includes"

	#include "arena.h"

	#include <stdarg.h>

#pragma endregion

#pragma region "Structures"

	typedef struct {
		char	*buffer;
		size_t	size;
		size_t	pos;
		int		error;
	} t_buffer;

#pragma endregion

#pragma region "Atomic Printf"

	#pragma region "Char"

		static void print_c_buf(int c, t_buffer *buf) {
			if (buf->error || buf->pos >= buf->size - 1) { buf->error = 1; return; }
			buf->buffer[buf->pos++] = c;
		}

	#pragma endregion

	#pragma region "String"

		static void print_s_buf(char *s, t_buffer *buf) {
			if (!s) s = "(null)";
			while (*s && !buf->error) print_c_buf(*s++, buf);
		}

	#pragma endregion

	#pragma region "Number N"

		static void print_n_buf(unsigned long n, unsigned int base, t_buffer *buf, int uppercase) {
			char *strbase = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

			if		(base == 11) {	print_c_buf('-', buf);	base = 10; }
			else if (base == 17) {	print_s_buf("0x", buf);	base = 16; }

			if (n >= base)			print_n_buf(n / base, base, buf, uppercase);
			if (!buf->error)		print_c_buf(strbase[n % base], buf);
		}

	#pragma endregion

	#pragma region "Number U"

		static void print_u_buf(size_t n, t_buffer *buf) {
			if (n >= 10)			print_u_buf(n / 10, buf);
			if (!buf->error)		print_c_buf("0123456789"[n % 10], buf);
		}

	#pragma endregion

	#pragma region "Chooser"

		static void chooser_buf(int c, va_list args, t_buffer *buf) {
			if		(c == '%')		print_c_buf('%', buf);
			else if (c == 'c')		print_c_buf(va_arg(args, int), buf);
			else if (c == 's')		print_s_buf(va_arg(args, char *), buf);
			else if (c == 'u')		print_u_buf(va_arg(args, size_t), buf);
			else if (c == 'p')		print_n_buf(va_arg(args, unsigned long), 17, buf, 0);
			else if (c == 'x')		print_n_buf(va_arg(args, unsigned int), 16, buf, 0);
			else if (c == 'X')		print_n_buf(va_arg(args, unsigned int), 16, buf, 1);
			else if (c == 'd' || c == 'i') {
				long long n = va_arg(args, int);
				if (n < 0)			print_n_buf(n * -1, 11, buf, 0);
				else				print_n_buf(n, 10, buf, 0);
			}
		}

	#pragma endregion

	#pragma region "Atomic Printf"

		int aprintf(int fd, int add_alloc_hist, char const *format, ...) {
			char buffer[4096];
			t_buffer buf = {
				.buffer = buffer,
				.size = sizeof(buffer),
				.pos = 0,
				.error = 0
			};

			va_list args;
			va_start(args, format);

			while (format && *format && !buf.error) {
				if (*format == '%')	chooser_buf(*(++format), args, &buf);
				else				print_c_buf(*format, &buf);
				format++;
			}

			va_end(args);

			mutex(&g_manager.hist_mutex, MTX_LOCK);

				if (buf.error && buf.pos > 0) buf.pos = buf.size - 1;
				if (buf.pos > 0) {
					if (add_alloc_hist && g_manager.options.LOGGING) {
						if (g_manager.hist_size < SIZE_MAX) {
							if (!g_manager.hist_size || g_manager.hist_pos + buf.pos >= g_manager.hist_size) heap_hist_extend();
							ft_memcpy(&g_manager.hist_buffer[g_manager.hist_pos], buffer, buf.pos);
							g_manager.hist_pos += buf.pos;
						}
					}
					if (fd == -1 || (add_alloc_hist && !g_manager.options.DEBUG && !g_manager.options.LOGGING)) {
						mutex(&g_manager.hist_mutex, MTX_UNLOCK);
						return (0);
					}

					int result = write(fd, buffer, buf.pos);

					mutex(&g_manager.hist_mutex, MTX_UNLOCK);
					return (result == -1 ? -1 : result);
				}

			mutex(&g_manager.hist_mutex, MTX_UNLOCK);

			return (0);
		}

	#pragma endregion

#pragma endregion
