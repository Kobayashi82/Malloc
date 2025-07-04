/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   valloc.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vzurera- <vzurera-@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/29 22:43:27 by vzurera-          #+#    #+#             */
/*   Updated: 2025/07/04 13:26:00 by vzurera-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma region "Includes"

	#include "arena.h"

#pragma endregion

#pragma region "Valloc"

	__attribute__((visibility("default")))
	void *valloc(size_t size) {
		ensure_init();

		void *ptr = NULL;

		if (!size) {
			mutex(&g_manager.mutex, MTX_LOCK);

				size_t aligned_offset = (g_manager.alloc_zero_counter * PAGE_SIZE);
				g_manager.alloc_zero_counter++;
				
			mutex(&g_manager.mutex, MTX_UNLOCK);

			ptr = (void*)(ZERO_MALLOC_BASE + aligned_offset);
			if (ptr && print_log(0))	aprintf(g_manager.options.fd_out, 1, "%p\t [VALLOC] Allocated %u bytes\n", ptr, size);
			else if (!ptr) errno = ENOMEM;

			return (ptr);
		}

		ptr = allocate_aligned("VALLOC", PAGE_SIZE, size);

		return (ptr);
	}

#pragma endregion

#pragma region "Information"

	// Allocates a page‑aligned block of memory.
	//
	//   void *valloc(size_t size);
	//
	//   size – the size of the memory block to allocate, in bytes.
	//
	//   • On success: returns a pointer aligned to the system page size.
	//   • On failure: returns NULL and sets errno to:
	//       – ENOMEM: not enough memory.
	//
	// Notes:
	//   • valloc() is non‑standard and obsolete; prefer posix_memalign() or aligned_alloc() for portable code.

#pragma endregion
