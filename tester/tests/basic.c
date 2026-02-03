/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   basic.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vzurera- <vzurera-@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/29 21:42:48 by vzurera-          #+#    #+#             */
/*   Updated: 2026/02/03 21:55:29 by vzurera-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma region "Includes"

	#include <malloc.h>
	#include <stdlib.h>
	#include <string.h>

#pragma endregion

#pragma region "Defines"

	#define M_DEBUG		7
	#define M_LOGGING	8

	void show_alloc_mem(void);
	void show_alloc_mem_ex(void *ptr, size_t offset, size_t length);
	void show_alloc_history(void);

#pragma endregion

#pragma region "Main"

	int main() {
		mallopt(M_DEBUG, 2);
		mallopt(M_LOGGING, 0);

		char *ptr = malloc(30);
		strcpy(ptr, "Hello World!\n");
		show_alloc_mem();
		show_alloc_mem_ex(ptr, 0, 0);
		free(ptr);
		show_alloc_history();
	}

#pragma endregion
