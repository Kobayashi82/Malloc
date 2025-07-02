/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   reallocarray.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vzurera- <vzurera-@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/28 12:37:30 by vzurera-          #+#    #+#             */
/*   Updated: 2025/07/02 22:35:54 by vzurera-         ###   ########.fr       */
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
			if (print_log(1))			aprintf(g_manager.options.fd_out, 1, "%p\t  [ERROR] Invalid pointer (not aligned)\n", ptr);
			if (print_error())			aprintf(2, 0, "reallocarray: Invalid pointer\n");
			return (1);
		}

		// Heap freed
		if (!heap->active) {
			if (heap->type == LARGE && GET_PTR(heap->ptr) == ptr) {
				if (print_log(1))		aprintf(g_manager.options.fd_out, 1, "%p\t  [ERROR] Invalid pointer (freed)\n", ptr);
				if (print_error())		aprintf(2, 0, "reallocarray: Invalid pointer\n");
			} else {
				if (print_log(1))		aprintf(g_manager.options.fd_out, 1, "%p\t  [ERROR] Invalid pointer (in middle of chunk)\n", ptr);
				if (print_error())		aprintf(2, 0, "reallocarray: Invalid pointer\n");
			}
			return (1);
		}

		// LARGE
		if (heap->type == LARGE) {
				if (GET_HEAD(ptr) != heap->ptr) {
				if (print_log(1))		aprintf(g_manager.options.fd_out, 1, "%p\t  [ERROR] Invalid pointer (in middle of chunk)\n", ptr);
				if (print_error())		aprintf(2, 0, "reallocarray: Invalid pointer\n");
				return (1);
			}
		}

		// Double free
		if (HAS_POISON(ptr)) {
			if (print_log(1))			aprintf(g_manager.options.fd_out, 1, "%p\t  [ERROR] Invalid pointer (freed)\n", ptr);
			if (print_error())			aprintf(2, 0, "reallocarray: Invalid pointer\n");
			return (1);
		}

		// In top chunk
		t_chunk *chunk = (t_chunk *)GET_HEAD(ptr);
		if ((chunk->size & TOP_CHUNK) && heap->type != LARGE) {
			if (print_log(1))			aprintf(g_manager.options.fd_out, 1, "%p\t  [ERROR] Invalid pointer (in top chunk)\n", ptr);
			if (print_error())			aprintf(2, 0, "reallocarray: Invalid pointer\n");
			return (1);
		}

		// In middle chunk
		if (heap->type != LARGE) {
			t_chunk *next_chunk = GET_NEXT(chunk);
			if (!(next_chunk->size & PREV_INUSE)) {
				if (print_log(1))		aprintf(g_manager.options.fd_out, 1, "%p\t  [ERROR] Invalid pointer (in middle of chunk)\n", ptr);
				if (print_error())		aprintf(2, 0, "reallocarray: Invalid pointer\n");
				return (1);
			}
		}

		return(0);
	}

#pragma endregion

#pragma region "Realloc Array"

	__attribute__((visibility("default")))
	void *reallocarray(void *ptr, size_t nmemb, size_t size) {
		ensure_init();

		if (nmemb && size && nmemb > SIZE_MAX / size) { errno = ENOMEM; return (NULL); }
		size = nmemb * size;
			
		if (!ptr)			return allocate("REALLOC_ARRAY", size, 0);	// ptr NULL is equivalent to malloc(size)
		if (!size)			return (free(ptr), NULL);					// size 0 is equivalent to free(ptr)
		if (!arena_find()) 	return (NULL);
		
		void	*new_ptr = NULL;
		bool	is_new = false;
		t_heap	*heap = NULL;

		mutex(&tcache->mutex, MTX_LOCK);

			heap = heap_find(tcache, ptr);
			if (!heap) {
				mutex(&tcache->mutex, MTX_UNLOCK);
				// return (native_realloc(ptr, size));
				return (NULL);
			}

			if (usable_ptr(ptr, heap)) {
				mutex(&tcache->mutex, MTX_UNLOCK);
				return (NULL);
			}

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
						link_chunk(new_chunk, remaining - sizeof(t_chunk), heap->type, tcache, heap);
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
								unlink_chunk(next, tcache, heap);
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
									link_chunk(new_chunk, remaining - sizeof(t_chunk), heap->type, tcache, heap);
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

		mutex(&tcache->mutex, MTX_UNLOCK);

		if (!new_ptr) {
			new_ptr = allocate("REALLOC_ARRAY", ALIGN(size + sizeof(t_chunk)), 0);
			if (new_ptr) {
				is_new = true;
				size_t copy_size = GET_SIZE((t_chunk *)GET_HEAD(ptr));
				ft_memcpy(new_ptr, ptr, (size < copy_size) ? size : copy_size);
			}
		}

		if (is_new) free(ptr);
		return (new_ptr);
	}

#pragma endregion

#pragma region "Information"

	// Reallocates memory for an array with overflow protection.
	//
	//   void *reallocarray(void *ptr, size_t nmemb, size_t size);
	//
	//   ptr   – pointer returned by malloc/calloc/realloc.
	//   nmemb – number of elements.
	//   size  – the size of each element, in bytes.
	//
	//   • On success: returns a pointer aligned for nmemb * size bytes (may be the same as ptr or a new location).
	//   • On failure: returns NULL, original block unchanged and sets errno to:
	//       – ENOMEM: not enough memory.
	//
	// Notes:
	//   • If nmemb == 0 || size == 0:
	//       – ptr != NULL → returns NULL, leaves ptr valid (not an error).
	//       – ptr == NULL → returns a unique pointer you can free (malloc(0)).

#pragma endregion
