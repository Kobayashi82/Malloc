/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   internal.h                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vzurera- <vzurera-@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/26 13:07:24 by vzurera-          #+#    #+#             */
/*   Updated: 2026/02/03 19:25:34 by vzurera-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#pragma region "Includes"

	#include "utils.h"

	#include <stdint.h>
	#include "unistd.h"
	#include <stdbool.h>
	#include <stdlib.h>
	#include <errno.h>
	#include <pthread.h>
	#include <dlfcn.h>
	#include <fcntl.h>
	#include <sys/mman.h>

#pragma endregion

#pragma region "Defines"

	#ifndef SIZE_MAX
		#define SIZE_MAX				~(size_t)0
	#endif
	#ifndef PATH_MAX
		#define PATH_MAX 				4096
	#endif
	
	#define ARCHITECTURE				(sizeof(size_t) * 8)																					// 32 or 64 bits

	// --- MAGIC / POISON ---
	#define MAGIC_BYTES					((size_t)0xABCDEF0123456789ULL)																			// Detect chunk corruption (4 or 8 bytes)
	#define POISON_BYTES				((size_t)0xDEADBEEFCAFEBABEULL)																			// Detect chunk corruption and double free (4 or 8 bytes)
	#define HAS_MAGIC(ptr)				(*(const size_t *)((char *)(ptr) - sizeof(size_t)) == MAGIC_BYTES)										// Check if chunk has MAGIC header
	#define HAS_POISON(ptr)				(*(const size_t *)((char *)(ptr) - sizeof(size_t)) == POISON_BYTES)										// Check if chunk has POISON pattern
	#define SET_MAGIC(ptr)				(*(size_t *)((char *)(ptr) - sizeof(size_t)) = MAGIC_BYTES)												// Set MAGIC header
	#define SET_POISON(ptr)				(*(size_t *)((char *)(ptr) - sizeof(size_t)) = POISON_BYTES)											// Set POISON pattern

	// --- FD ---
	#define GET_FD(chunk)				*(void **)((char *)(chunk) + sizeof(t_chunk))															// Get forward pointer
	#define SET_FD(chunk, next_chunk)	(*(void **)((char *)(chunk) + sizeof(t_chunk)) = (next_chunk))											// Set forward pointer

	// --- CHUNK ---
	#define GET_PTR(chunk)				(void *)((char *)(chunk) + sizeof(t_chunk))																// Get pointer to user data
	#define GET_HEAD(chunk) 			(void *)((char *)(chunk) - sizeof(t_chunk))																// Get pointer to chunk header
	#define GET_NEXT(chunk)				(t_chunk *)((char *)(chunk) + sizeof(t_chunk) + ((chunk)->size & ~15))									// Get pointer to next chunk header
	#define GET_PREV(chunk)				(t_chunk *)((char *)(chunk) - (*(uint32_t *)((char *)(chunk) - sizeof(uint32_t)) + sizeof(t_chunk)))	// Get pointer to previous chunk header
	#define GET_PREV_SIZE(chunk)		(*(uint32_t *)((char *)(chunk) - sizeof(uint32_t)))														// Get size of previous chunk (without header)
	#define SET_PREV_SIZE(chunk, size)	(*(uint32_t *)((char *)(chunk) - sizeof(uint32_t)) = (size))											// Set size of previous chunk (without header)
	#define GET_SIZE(chunk) 			(size_t)((chunk)->size & ~15)																			// Get size of chunk (without header)
	#define IS_TOPCHUNK(chunk)			(((chunk)->size & TOP_CHUNK) != 0)																		// Check if chunk is the top chunk
	#define IS_FREE(chunk)				(((GET_NEXT(chunk))->size & PREV_INUSE) == 0)															// Check if chunk is free

	// --- CHUNK FLAGS ---
	#define MMAP_CHUNK    				0x8																										// Bit 3: chunk comes from mmap (LARGE)
	#define TOP_CHUNK					0x4																										// Bit 2: chunk is the top chunk
	#define HEAP_TYPE					0x2																										// Bit 1: indicates heap type
	#define PREV_INUSE					0x1																										// Bit 0: previous chunk is in use

	// --- ALIGNMENTS ---
	#define ZERO_MALLOC_BASE			(void *)0x100000000000																					// Base address returned by malloc(0) to distinguish from NULL
	#define PAGE_SIZE					get_pagesize()																							// System page size (usually 4096 bytes)
	#define ALIGNMENT					(uint8_t)(ARCHITECTURE / 4)																				// Global alignment (8 or 16 bytes)
	#define IS_ALIGNED(ptr)				(((uintptr_t)GET_HEAD(ptr) & (ALIGNMENT - 1)) == 0)														// Check if header is properly aligned
	#define ALIGN(size)					(((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))															// Align size up to ALIGNMENT
	#define ALIGN_UP(addr, align)		(((addr) + (align) - 1) & ~((align) - 1))																// Align address upwards to the nearest multiple of 'align'

	// --- HEAP SIZES ---
	#define TINY_CHUNK					512																										// Max size for tiny chunk
	#define TINY_BLOCKS					128																										// Number of tiny chunks per HEAP
	#define TINY_SIZE					(((TINY_BLOCKS * TINY_CHUNK) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))										// Total size of tiny heap, aligned to page

	#define SMALL_CHUNK					4096																									// Max size for small chunk
	#define SMALL_BLOCKS				128																										// Number of small chunks per HEAP
	#define SMALL_SIZE					(((SMALL_BLOCKS * SMALL_CHUNK) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))										// Total size of small heap, aligned to page

	// --- HEAP REMOVAL ---
	#define FREE_PERCENT				10.0f																									// Max % of free memory in other heaps required to consider remove a heap
	#define FRAG_PERCENT				90.0f																									// Minf % of ragmentation in other heaps required to consider remove a heap

#pragma endregion

#pragma region "Enumerators"

	enum {
		TINY,
		SMALL,
		LARGE,
		MTX_INIT,
		MTX_LOCK,
		MTX_TRYLOCK,
		MTX_UNLOCK,
		MTX_DESTROY
	};

#pragma endregion

#pragma region "Structures"

	typedef struct s_chunk {
		size_t			size;						// Size of the user data (include chunk flags)
		size_t			magic;						// MAGIC header if in use, POISON pattern if freed
	} t_chunk;

	typedef struct s_heap_header {
		uint8_t 		total;						// Max heap info that can be stored in a pagefile. A pagefile can store 73 or 85 heaps depending if there is arena info in that page
		uint8_t 		used;						// Number of heap info stored
		void			*next;						// Pointer to next heap header (in another pagefile)
	} t_heap_header;

	typedef struct s_heap {
		void			*ptr;						// Pointer to the start of the heap (first chunk)
		size_t			padding;					// Unused space used as padding for the user pointer so it is aligned (only used in LARGE heaps)
		size_t			size;						// Size of the heap (not including padding)
		size_t			free;						// Memory available for allocation in the heap (not including padding)
		uint16_t		free_chunks;				// Number of free chunks in the heap
		bool			active;						// Indicate if the heap is in used. Set to false when freed (used to detect double free)
		int				type;						// Type of the heap (TINY, SMALL or LARGE)
		t_chunk			*top_chunk;					// Pointer to the top chunk (unused memory at the end)
	} t_heap;

	typedef struct s_arena {
		int				id;							// Arena ID (0 = main thread)
		int				alloc_count;				// Total number of allocations
		int				free_count;					// Total number of frees
		void			*bins[257];					// Bins
		t_heap_header	*heap_header;				// Pointer to the first heap header
		struct s_arena	*next;          			// Pointer to the next arena
		pthread_mutex_t	mutex;          			// Arena mutex for thread safety
	} t_arena;

	typedef struct s_options {
		int				MIN_USAGE;					// Heaps under this usage % are skipped (unless all are under)
		int				CHECK_ACTION;				// Behaviour on abort errors (0: abort, 1: warning, 2: silence)
		unsigned char	PERTURB;					// Sets memory to the PERTURB value on allocation, and to value ^ 255 on free
		int				ARENA_TEST;					// Number of arenas at which a hard limit on arenas is computed
		int				ARENA_MAX;					// Maximum number of arenas allowed
		int				DEBUG;						// Enables debug mode (1: error, 2: system)
		int				LOGGING;					// Enables logging mode (1: to file, 2: to stderr)
		char 			LOGFILE[PATH_MAX];			// Log file path
		int				fd_out;						// File descriptor used for logs or output
	} t_options;

	typedef struct s_manager {
		int				arena_count;				// Number of arenas created
		t_options		options;					// Global configuration options
		t_arena			arena;						// Main arena (thread 0)
		size_t			alloc_zero_counter;			// Counter for alloc calls
		char			*hist_buffer;				// History buffer
		size_t			hist_size;					// Size of history buffer
		size_t			hist_pos;					// Current write position in history buffer
		pthread_mutex_t	hist_mutex;					// History mutex for thread safety
		pthread_mutex_t	mutex;						// Global mutex for thread safety
	} t_manager;

#pragma endregion

#pragma region "Variables"

	extern __thread t_arena	*tcache;				// Thread-local arena
	extern t_manager		g_manager;				// Main structure

#pragma endregion

#pragma region "Methods"

	// Internal
	int		mutex(pthread_mutex_t *ptr_mutex, int action);
	void	*internal_alloc(size_t size);
	int		internal_free(void *ptr, size_t size);
	bool	print_log(int mode);
	bool	print_error();
	int		abort_now();
	void	ensure_init();
	size_t	get_pagesize();

	// Options
	void	options_initialize();
	int		options_set(int param, int value);

#pragma endregion
