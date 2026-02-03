/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   realloc.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vzurera- <vzurera-@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/18 11:32:56 by vzurera-          #+#    #+#             */
/*   Updated: 2026/02/03 18:18:18 by vzurera-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma region "Includes"

	#include "arena.h"

#pragma endregion

#pragma region "Usable PTR"

	static int usable_ptr(void *ptr, t_heap *heap) {
		if (!ptr || !heap) return (1);

		// Not aligned
		if (!IS_ALIGNED(ptr)) {
			if (print_log(1))			aprintf(g_manager.options.fd_out, 1, "%p\t  [ERROR] Invalid pointer (realloc: not aligned)\n", ptr);
			if (print_error())			aprintf(2, 0, "realloc: Invalid pointer\n");
			return (abort_now());
		}

		// Heap freed
		if (!heap->active) {
			if (heap->type == LARGE && GET_PTR(heap->ptr) == ptr) {
				if (print_log(1))		aprintf(g_manager.options.fd_out, 1, "%p\t  [ERROR] Invalid pointer (realloc: poison)\n", ptr);
				if (print_error())		aprintf(2, 0, "realloc: Invalid pointer\n");
			} else {
				if (print_log(1))		aprintf(g_manager.options.fd_out, 1, "%p\t  [ERROR] Invalid pointer (realloc: in middle of chunk)\n", ptr);
				if (print_error())		aprintf(2, 0, "realloc: Invalid pointer\n");
			}
			return (abort_now());
		}

		// LARGE
		if (heap->type == LARGE) {
				if (GET_HEAD(ptr) != heap->ptr) {
				if (print_log(1))		aprintf(g_manager.options.fd_out, 1, "%p\t  [ERROR] Invalid pointer (realloc: in middle of chunk)\n", ptr);
				if (print_error())		aprintf(2, 0, "realloc: Invalid pointer\n");
				return (abort_now());
			}
		}

		// Double free
		if (HAS_POISON(ptr)) {
			if (print_log(1))			aprintf(g_manager.options.fd_out, 1, "%p\t  [ERROR] Invalid pointer (realloc: poison)\n", ptr);
			if (print_error())			aprintf(2, 0, "realloc: Invalid pointer\n");
			return (abort_now());
		}

		// In top chunk
		t_chunk *chunk = (t_chunk *)GET_HEAD(ptr);
		if ((chunk->size & TOP_CHUNK) && heap->type != LARGE) {
			if (print_log(1))			aprintf(g_manager.options.fd_out, 1, "%p\t  [ERROR] Invalid pointer (realloc: in top chunk)\n", ptr);
			if (print_error())			aprintf(2, 0, "realloc: Invalid pointer\n");
			return (abort_now());
		}

		// In middle chunk
		if (heap->type != LARGE) {
			t_chunk *next_chunk = GET_NEXT(chunk);
			if (!(next_chunk->size & PREV_INUSE)) {
				if (print_log(1))		aprintf(g_manager.options.fd_out, 1, "%p\t  [ERROR] Invalid pointer (realloc: in middle of chunk)\n", ptr);
				if (print_error())		aprintf(2, 0, "realloc: Invalid pointer\n");
				return (abort_now());
			}
		}

		return(0);
	}

#pragma endregion

#pragma region "Realloc"

	__attribute__((visibility("default")))
	void *realloc(void *ptr, size_t size) {
		ensure_init();

		if (!ptr)			return allocate("REALLOC", size);			// ptr NULL is equivalent to malloc(size)
		if (!size)			return (free(ptr), NULL);					// size 0 is equivalent to free(ptr)
		if (!arena_find()) 	return (NULL);

		// alloc zero
		if (check_digit(ptr, ZERO_MALLOC_BASE)) {
			mutex(&g_manager.mutex, MTX_LOCK);

				if (ptr > ZERO_MALLOC_BASE && ptr < (void *)((char *)ZERO_MALLOC_BASE + (g_manager.alloc_zero_counter * ALIGNMENT))) {
					mutex(&g_manager.mutex, MTX_UNLOCK);
					return allocate("REALLOC", size);
				}
			
			mutex(&g_manager.mutex, MTX_UNLOCK);
		}

		void	*new_ptr = NULL;
		bool	is_new = false;
		t_heap	*heap = NULL;
		t_arena	*arena = &g_manager.arena;
		size_t	old_size = 0;

		mutex(&g_manager.mutex, MTX_LOCK);

			while (arena) {
				mutex(&arena->mutex, MTX_LOCK);

					if ((heap = heap_find(arena, ptr)) && heap->active) {
						mutex(&arena->mutex, MTX_UNLOCK);
						break;
					}

				mutex(&arena->mutex, MTX_UNLOCK);
				arena = arena->next;
			}

		mutex(&g_manager.mutex, MTX_UNLOCK);

		if (!arena || !heap || !heap->active) {
			if (print_log(1))		aprintf(g_manager.options.fd_out, 1, "%p\t  [ERROR] Invalid pointer (realloc: heap may be unmamped)\n", ptr);
			if (print_error())		aprintf(2, 0, "realloc: Invalid pointer\n");
			return (abort_now(), NULL);
		}

		mutex(&arena->mutex, MTX_LOCK);

			if (usable_ptr(ptr, heap)) {
				mutex(&arena->mutex, MTX_UNLOCK);
				return (abort_now(), NULL);
			}

			old_size = GET_SIZE((t_chunk *)GET_HEAD(ptr));
			t_chunk *chunk = GET_HEAD(ptr);
			size_t	chunk_size = GET_SIZE(chunk);
			if (ALIGN(size) <= chunk_size) {
				if (heap->type == LARGE) new_ptr = ptr;
				else {
					size_t remaining = chunk_size - ALIGN(size);
					if (remaining >= sizeof(t_chunk) + ((heap->type == TINY) ? 48 : TINY_CHUNK)) {
						chunk->size = (chunk->size & (HEAP_TYPE | PREV_INUSE)) | ALIGN(size);
						t_chunk *new_chunk = (t_chunk *)((char *)chunk + ALIGN(size) + sizeof(t_chunk));
						SET_PREV_SIZE(new_chunk, ALIGN(size));
						SET_POISON(GET_PTR(new_chunk));
						new_chunk->size = (remaining - sizeof(t_chunk)) | ((heap->type == SMALL) ? HEAP_TYPE : 0) | PREV_INUSE;
						t_chunk *next_chunk = GET_NEXT(new_chunk);
						SET_PREV_SIZE(next_chunk, remaining - sizeof(t_chunk));
						next_chunk->size &= ~PREV_INUSE;
						link_chunk(new_chunk, arena, heap);
						heap->free += remaining;
						new_ptr = ptr;
					} else new_ptr = ptr;
				}
			} else if (heap->type != LARGE) {
				size_t needed_size = ALIGN(size);
				size_t current_size = GET_SIZE(chunk);

				if (needed_size > current_size) {
					size_t extra_needed = needed_size - current_size;
					size_t absorbed = 0;
					t_chunk *next = GET_NEXT(chunk);
					bool can_extend = false;
					
					if (needed_size < ((heap->type == TINY) ? TINY_CHUNK : SMALL_CHUNK)) {
						while (next && absorbed < extra_needed) {
							if (!IS_TOPCHUNK(next) && !IS_FREE(next)) break;

							if (IS_TOPCHUNK(next)) {
								next = split_top_chunk(heap, ALIGN((extra_needed - absorbed) + sizeof(t_chunk)));
								if (next) {
									absorbed += GET_SIZE(next) + sizeof(t_chunk);
									can_extend = true;
									heap->free -= GET_SIZE(next) + sizeof(t_chunk);
								}
								break;
							} else {
								unlink_chunk(next, arena, heap);
								absorbed += GET_SIZE(next) + sizeof(t_chunk);
								if (absorbed >= extra_needed) { can_extend = true; break; }
								heap->free_chunks--;
								heap->free -= GET_SIZE(next) + sizeof(t_chunk);
								next = GET_NEXT(next);
							}
						}

						if (can_extend && absorbed >= extra_needed) {

							chunk->size = (chunk->size & (HEAP_TYPE | PREV_INUSE)) | (current_size + absorbed);

							if (current_size + absorbed > ALIGN(size)) {
								size_t remaining = (current_size + absorbed) - ALIGN(size);
								if (remaining >= sizeof(t_chunk) + ((heap->type == TINY) ? 48 : TINY_CHUNK)) {
									chunk->size = (chunk->size & (HEAP_TYPE | PREV_INUSE)) | ALIGN(size);
									t_chunk *new_chunk = (t_chunk *)((char *)chunk + ALIGN(size) + sizeof(t_chunk));
									SET_PREV_SIZE(new_chunk, ALIGN(size));
									SET_POISON(GET_PTR(new_chunk));
									new_chunk->size = (remaining - sizeof(t_chunk)) | ((heap->type == SMALL) ? HEAP_TYPE : 0) | PREV_INUSE;
									t_chunk *next_chunk = GET_NEXT(new_chunk);
									SET_PREV_SIZE(next_chunk, remaining - sizeof(t_chunk));
									next_chunk->size &= ~PREV_INUSE;
									link_chunk(new_chunk, arena, heap);
									heap->free += remaining;
								}
							}

							t_chunk *new_next = GET_NEXT(chunk);
							if (new_next) {
								SET_PREV_SIZE(new_next, GET_SIZE(chunk));
								new_next->size |= PREV_INUSE;
							}

							new_ptr = ptr;
						}
					}
				} else new_ptr = ptr;
			}

		mutex(&arena->mutex, MTX_UNLOCK);

		if (new_ptr == ptr && old_size && print_log(0)) {
			size_t req_size = ALIGN(size);
			if (req_size > old_size)
				aprintf(g_manager.options.fd_out, 1, "%p\t [REALLOC] Extended to %u bytes\n", ptr, req_size);
			else if (req_size < old_size)
				aprintf(g_manager.options.fd_out, 1, "%p\t [REALLOC] Shrunk to %u bytes\n", ptr, req_size);
			else
				aprintf(g_manager.options.fd_out, 1, "%p\t [REALLOC] Size unchanged %u bytes\n", ptr, req_size);
		}

		if (new_ptr && g_manager.options.PERTURB && heap->type != LARGE) {
			size_t new_size = GET_SIZE((t_chunk *)GET_HEAD(new_ptr));
			if (new_size > old_size) {
				size_t len = new_size - old_size;
				ft_memset((char *)new_ptr + old_size, g_manager.options.PERTURB ^ 0xFF, len);
			}
		}

		if (!new_ptr) {
			new_ptr = allocate("REALLOC", ALIGN(size + sizeof(t_chunk)));
			if (new_ptr) {
				is_new = true;
				ft_memcpy(new_ptr, ptr, (size < old_size) ? size : old_size);
			}
		}

		if (is_new) free(ptr);
		return (new_ptr);
	}

#pragma endregion

#pragma region "Information"

	// Resizes a previously allocated memory block.
	//
	//   void *realloc(void *ptr, size_t size);
	//
	//   ptr   – pointer returned by malloc/calloc/realloc.
	//   size  – the new size of the memory block, in bytes.
	//
	//   • On success: returns a pointer aligned for the requested size (may be the same as ptr or a new location).
	//   • On failure: returns NULL, original block unchanged and sets errno to:
	//       – ENOMEM: not enough memory.
	//
	// Notes:
	//   • If size == 0:
	//       – ptr != NULL → returns NULL and free ptr.
	//       – ptr == NULL → behaves like malloc(size).

#pragma endregion
