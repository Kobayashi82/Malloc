/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   bin.c                                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vzurera- <vzurera-@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/28 22:11:21 by vzurera-          #+#    #+#             */
/*   Updated: 2025/06/12 00:08:36 by vzurera-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma region "Includes"

	#include "arena.h"

#pragma endregion

#pragma region "Top Chunk"

	#pragma region "Split"

		static t_chunk *split_top_chunk(t_heap *heap, t_chunk_int size) {
			if (!heap || !size) return (NULL);

			t_chunk *top_chunk = heap->top_chunk;
			if (!top_chunk) return (NULL);

			size_t top_chunk_available = GET_SIZE(top_chunk);
			if (top_chunk_available < (size_t)size) return (NULL);

			t_chunk *chunk = top_chunk;
			chunk->size = (top_chunk->size & (HEAP_TYPE | PREV_INUSE)) | (size - sizeof(t_chunk));
			chunk->size &= ~TOP_CHUNK;

			top_chunk = GET_NEXT(chunk);
			top_chunk->size = (top_chunk_available - size) | TOP_CHUNK | ((heap->type == SMALL) ? HEAP_TYPE : 0) | PREV_INUSE;
			top_chunk->prev_size = size - sizeof(t_chunk);
			heap->top_chunk = top_chunk;

			return (chunk);
		}

	#pragma endregion

	#pragma region "New Chunk"

		static void *new_chunk(t_arena *arena, t_chunk_int size, e_heaptype type) {
			if (!arena || !size || type < TINY || type > SMALL) return (NULL);

			void	*ptr = NULL;
			bool	created = false;

			// Find first heap
			size_t	heap_size = (type == TINY) ? TINY_SIZE : SMALL_SIZE;
			t_heap	*heap = (type == TINY) ? arena->tiny : arena->small;
			if (!heap) {
				created = true;
				heap_create(type, heap_size);
				heap = (type == TINY) ? arena->tiny : arena->small;
				if (!heap) return (ptr);
			}
			// Find best heap
			t_heap	*best_heap = NULL;
			float	best_usage = -1000;
			
			while (heap) {
				bool	available = heap->free >= size;
				int		usage = ((heap_size - heap->free) * 100) / heap_size;

				if (available && usage > 10) { best_heap = heap; break; }
				if (available && usage > best_usage) { best_usage = usage; best_heap = heap; }
				heap = heap->next;
			}

			// Create heap (no best heap found)
			if (!best_heap) {
				created = true;
				heap_create(type, heap_size);
				best_heap = (type == TINY) ? arena->tiny : arena->small;
				if (!best_heap) return (ptr);
			}

			// Split top chunk
			t_chunk	*chunk = split_top_chunk(best_heap, size);
			if (!chunk && !created) {
				heap_create(type, heap_size);
				best_heap = (type == TINY) ? arena->tiny : arena->small;
				if (!best_heap) return (ptr);
				chunk = split_top_chunk(best_heap, size);
				if (!chunk) return (ptr);
			}

			best_heap->free -= size;
			ptr = (GET_PTR(chunk));

			return (ptr);
		}

	#pragma endregion

#pragma endregion

#pragma region "Find"

	#pragma region "FastBin"

		static void *find_in_fastbin(t_arena *arena, size_t size) {
			if (!arena || !size) return (NULL);
				
			void *ptr = NULL;

			int index = (size - 1) / ALIGNMENT;
			if (index >= g_manager.options.MXFAST / ALIGNMENT) return (ptr);
			if (arena->fastbin[index]) {
				t_chunk *chunk = (t_chunk *)arena->fastbin[index];
				arena->fastbin[index] = GET_FD(chunk);
				t_chunk *next = GET_NEXT(chunk);
				next->size |= PREV_INUSE;

				ptr = GET_PTR(chunk);
				if (g_manager.options.DEBUG) aprintf(1, "%p\t [SYSTEM] Fastbin match for size %d bytes\n", ptr, size);
				return (ptr);
			}

			return (ptr);
		}

	#pragma endregion

	#pragma region "SmallBin"

		// static void *find_in_smallbin(t_arena *arena, size_t size) {
		// 	if (!arena || !size) return (NULL);

		// 	void *ptr = NULL;

		// 	return (ptr);
		// }

	#pragma endregion

#pragma endregion

#pragma region "Find in Bin"

	void *find_memory(t_arena *arena, size_t size) {
		if (!arena || !size) return (NULL);

		void	*ptr = NULL;
		size_t	align_size = ALIGN(size + sizeof(t_chunk));

		if (align_size <= (size_t)g_manager.options.MXFAST) ptr = find_in_fastbin(arena, align_size);
		// if (!ptr && align_size <= MAX_SIZE_BIN) ptr = find_in_smallbin(arena, align_size);
		// if (!ptr) ptr = find_in_unsortedbin(arena, align_size);
		// if (!ptr) ptr = find_in_largebin(arena, align_size);
		// if (!ptr && fastbin no vacio, repite
		// if (!ptr) ptr = find_in_largebin(arena, align_size);
		if (!ptr) ptr = new_chunk(arena, align_size, (align_size > TINY_USER) ? SMALL : TINY);

		if (ptr) arena->alloc_count++;

		return (ptr);
	}

#pragma endregion

#pragma region "Information"

	// cantidad_fastbins = MXFAST / ALIGNMENT
	// tamaño_del_chunk = (índice + 1) * ALIGNMENT
	// tamaño_máximo_real = cantidad_fastbins * ALIGNMENT

	// chunk->size |= PREV_INUSE;			// Activar bit
	// chunk->size &= ~PREV_INUSE;			// Desactivar bit
	// if (chunk->size & PREV_INUSE) {		// Comprobar bit
	// chunk->size ^= PREV_INUSE;			// Alternar bit
	// chunk->size & ~7;					// Limpiar bits

#pragma endregion
