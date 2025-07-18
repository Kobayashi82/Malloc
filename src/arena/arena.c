/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   arena.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vzurera- <vzurera-@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/19 23:58:18 by vzurera-          #+#    #+#             */
/*   Updated: 2025/07/05 15:36:28 by vzurera-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma region "Includes"

	#include "arena.h"

#pragma endregion

#pragma region "Initialize"

	void arena_initialize(t_arena *arena) {
		arena->id = g_manager.arena_count++;
		arena->alloc_count = 0;
		arena->free_count = 0;
		ft_memset(arena->bins, 0, 257 * sizeof(void *));
		arena->heap_header = NULL;
		arena->next = NULL;
		mutex(&arena->mutex, MTX_INIT);
	}

#pragma endregion

#pragma region "Find"

	t_arena *arena_find() {
		if (!tcache) {
			tcache = arena_get();
			if (!tcache) {
				if (print_log(1)) aprintf(g_manager.options.fd_out, 1, "\t\t  [ERROR] Failed to assign arena\n");
				errno = ENOMEM; return (NULL);
			}
		}

		return (tcache);
	}

#pragma endregion

#pragma region "Create"

	#pragma region "Get CPUs"

		int get_CPUs() {
			static int CPUs = 0;

			if (!CPUs) {
				int cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
				CPUs = (cores <= 0 ? 1 : cores);
			}

			return (CPUs);
		}

	#pragma endregion

	#pragma region "Can Create"

		static int arena_can_create() {
			if (!g_manager.options.ARENA_MAX) {
				if (g_manager.arena_count >= g_manager.options.ARENA_TEST) {
					g_manager.options.ARENA_MAX = ft_min(get_CPUs() * 2, ARCHITECTURE * 2);
					return (g_manager.arena_count < g_manager.options.ARENA_MAX);
				} else return (1);
			}
			return (g_manager.arena_count < g_manager.options.ARENA_MAX);
		}

	#pragma endregion

	#pragma region "Create"

		t_arena *arena_create() {
			if (!arena_can_create()) return (NULL);

			t_arena *new_arena = internal_alloc(PAGE_SIZE);
			if (!new_arena) return (NULL);
			arena_initialize((t_arena *)new_arena);

			t_arena *current = &g_manager.arena;
			while (current->next) current = current->next;
			current->next = new_arena;

			if (print_log(2)) aprintf(g_manager.options.fd_out, 1, "\t\t [SYSTEM] Arena #%d created\n", new_arena->id);

			return (new_arena);
		}

	#pragma endregion

#pragma endregion

#pragma region "Reuse"

	t_arena *arena_reuse() {
		t_arena *current, *best_arena = NULL;

		current = &g_manager.arena;
		while (current) {
			if (!mutex(&current->mutex, MTX_TRYLOCK)) {
					best_arena = current;
				mutex(&current->mutex, MTX_UNLOCK);
				// return (NULL);	// Force arena creation
				break;
			}
			current = current->next;
		}

		return (best_arena);
	}

#pragma endregion

#pragma region "Get"

	t_arena *arena_get() {
		static bool initialized;
		t_arena *arena = NULL;

		mutex(&g_manager.mutex, MTX_LOCK);

			if (!initialized) {
				initialized = true;
				arena_initialize(&g_manager.arena);
				arena = &g_manager.arena;
				if (print_log(2))	aprintf(g_manager.options.fd_out, 1, "\t\t [SYSTEM] Arena #%d created\n", arena->id);
			}
			if (!arena) arena = arena_reuse();
			if (!arena) arena = arena_create();
			if (!arena) arena = &g_manager.arena;

			if (print_log(2))		aprintf(g_manager.options.fd_out, 1, "\t\t [SYSTEM] Arena #%d assigned\n", arena->id);

		mutex(&g_manager.mutex, MTX_UNLOCK);

		return (arena);
	}

#pragma endregion
