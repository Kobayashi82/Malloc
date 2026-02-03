#!/bin/bash

# ────────────── Get script directory ──────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ────────────── Colors ───────────────────
RED='\033[0;31m'; BROWN='\033[0;33m'; YELLOW='\033[1;33m'; GREEN='\033[0;32m'; BLUE='\033[0;34m'; CYAN='\033[0;36m'; MAGENTA='\033[1;35m'; GREENB='\033[1;32m'; NC='\033[0m'

# ────────────── Compilation ──────────────
tests=(test0 test1 test2 test3 test4 test5 test6 test7)
for t in "${tests[@]}"; do
	file="${SCRIPT_DIR}/evaluation/${t}.c"
	[[ -f "$file" ]] || { echo -e "${RED}Missing $file${NC}"; exit 1; }
	
	if [[ "$t" == "test4" || "$t" == "test6" || "$t" == "test7" ]]; then
		clang "$file" -o "${SCRIPT_DIR}/evaluation/$t" -L"${SCRIPT_DIR}/../lib" -I"${SCRIPT_DIR}/../inc" -lft_malloc || { echo -e "${RED}Compilation failed $file${NC}"; exit 1; }
	else
		clang "$file" -o "${SCRIPT_DIR}/evaluation/$t" || { echo -e "${RED}Compilation failed $file${NC}"; exit 1; }
	fi
done
echo

# ────────────── Helpers ──────────────────
run() {
	tmp=$(mktemp)
	stdout_tmp=$(mktemp)
	/usr/bin/time -v "$1" 2>"$tmp" >"$stdout_tmp"
	exit_code=$?
	mem=$(grep "Maximum resident" "$tmp" | awk '{print $NF}')
	minor=$(grep "Minor" "$tmp" | awk '{print $NF}')
	
	# Check if stdout has two identical lines (for test3)
	realloc_ok=0
	if [[ "$1" == *"test3"* ]]; then
		mapfile -t lines < "$stdout_tmp"
		if [[ ${#lines[@]} -eq 2 && "${lines[0]}" == "${lines[1]}" ]]; then
			realloc_ok=1
		fi
	fi
	
	# Check if process aborted (for test4)
	abort_ok=0
	if [[ "$1" == *"test4"* ]]; then
		if (( exit_code != 0 )); then
			abort_ok=1
		fi
	fi
	
	rm -f "$tmp" "$stdout_tmp"
	((mem==0)) && echo "0 0 0 0 0" && return
	echo "$mem $((mem/4)) $minor $realloc_ok $abort_ok"
}

rate_pages_over_real() {
	local diff=$1
	(( diff < 0 )) && echo 0 && return
	(( diff >= 181 )) && echo 1 && return
	(( diff >= 91 )) && echo 2 && return
	(( diff >= 51 )) && echo 3 && return
	(( diff >= 21 )) && echo 4 && return
	echo 5
}

rate_free() {
	local t2_min=$1 t0_min=$2
	local diff=$(( t2_min - t0_min ))

	(( diff <= 10 )) && echo 5 && return
	echo 0
}

get_color() {
	local score=$1 max_score=$2
	local percent=$(( score * 100 / max_score ))
	
	(( percent >= 90 )) && echo "$GREEN" && return
	(( percent >= 70 )) && echo "$YELLOW" && return
	(( percent >= 30 )) && echo "$BROWN" && return
	echo "$RED"
}

# ────────────── Executions ───────────────
declare -A N_MEM N_PAGES N_MIN N_REALLOC N_ABORT C_MEM C_PAGES C_MIN C_REALLOC C_ABORT
for t in "${tests[@]}"; do
	unset LD_PRELOAD
	unset MALLOC_CHECK_;
	unset MALLOC_DEBUG MALLOC_LOGGING
	unset MALLOC_DEBUG
	unset MALLOC_LOGGING
	# native malloc
	read mem pages minor realloc abort < <(run "${SCRIPT_DIR}/evaluation/$t")
	N_MEM[$t]=$mem; N_PAGES[$t]=$pages; N_MIN[$t]=$minor; N_REALLOC[$t]=$realloc; N_ABORT[$t]=$abort
	# custom malloc
	unset MALLOC_DEBUG MALLOC_LOGGING
	export LD_LIBRARY_PATH="${SCRIPT_DIR}/../lib:$LD_LIBRARY_PATH"
	export LD_PRELOAD="libft_malloc.so"
	read mem pages minor realloc abort < <(run "${SCRIPT_DIR}/evaluation/$t")
	C_MEM[$t]=$mem; C_PAGES[$t]=$pages; C_MIN[$t]=$minor; C_REALLOC[$t]=$realloc; C_ABORT[$t]=$abort
	unset LD_PRELOAD
	unset MALLOC_CHECK_;
done

BASE_PAGES=${C_PAGES[test0]}
BASE_MIN=${C_MIN[test0]}

# ==================================================================================================
#                                          NATIVE MALLOC
# ==================================================================================================

title="NATIVE MALLOC"
pad=$(( (39 - ${#title}) / 2 ))
printf "%*s%b%s%b\n" "$pad" "" "$MAGENTA" "$title" "$NC"
echo
echo -en "${CYAN}"
printf "%-5s %12s %10s %10s\n" "TEST" "RSS (KB)" "RSS (Pg)" "Minor"
echo -e "${CYAN}----------------------------------------${NC}"
printf "%-5s ${GREEN}%12d${NC} ${YELLOW}%10d${NC} ${BROWN}%10d${NC}\n" "test0" "${N_MEM[test0]}" "${N_PAGES[test0]}" "${N_MIN[test0]}"

for t in test1 test2 test3 test4; do
	delta_mem=$(( N_MEM[$t] - N_MEM[test0] ))
	delta_pages=$(( N_PAGES[$t] - N_PAGES[test0] ))
	delta_min=$(( N_MIN[$t] - N_MIN[test0] ))
	
	printf "%-5s ${GREEN}%12d${NC} ${YELLOW}%10d${NC} ${BROWN}%10d${NC}\n" "$t" "${N_MEM[$t]}" "${N_PAGES[$t]}" "${N_MIN[$t]}"
done
echo

echo

# ==================================================================================================
#                                          CUSTOM MALLOC
# ==================================================================================================

title="CUSTOM MALLOC"
pad=$(( (39 - ${#title}) / 2 ))
printf "%*s%b%s%b\n" "$pad" "" "$GREENB" "$title" "$NC"
echo
echo -en "${CYAN}"
printf "%-5s %12s %10s %10s\n" "TEST" "RSS (KB)" "RSS (Pg)" "Minor"
echo -e "${CYAN}----------------------------------------${NC}"
display_min_base=${C_MIN[test0]}
if (( display_min_base < N_MIN[test0] )); then
	display_min_base=${N_MIN[test0]}
fi
printf "%-5s ${GREEN}%12d${NC} ${YELLOW}%10d${NC} ${BROWN}%10d${NC}\n" "test0" "${C_MEM[test0]}" "${C_PAGES[test0]}" "${display_min_base}"

for t in test1 test2 test3 test4; do
	delta_mem=$(( C_MEM[$t] - C_MEM[test0] ))
	delta_pages=$(( C_PAGES[$t] - C_PAGES[test0] ))
	display_min=${C_MIN[$t]}
	if (( display_min < N_MIN[$t] )); then
		display_min=${N_MIN[$t]}
	fi
	printf "%-5s ${GREEN}%12d${NC} ${YELLOW}%10d${NC} ${BROWN}%10d${NC}\n" "$t" "${C_MEM[$t]}" "${C_PAGES[$t]}" "${display_min}"
done
echo

# ────────────── Score ────────────────────
delta_min_test1=$(( (C_MIN[test1] - C_MIN[test0]) - (N_MIN[test1] - N_MIN[test0]) ))
(( delta_min_test1 < 0 )) && delta_min_test1=0
score1=$(rate_pages_over_real "${delta_min_test1}")
color1=$(get_color $score1 5)
if (( delta_min_test1 > 0 )); then
	delta_min_str="+${delta_min_test1}"
else
	delta_min_str="+0"
fi
echo -e "Memory score: ${color1}${score1}/5${NC} (${delta_min_str} minor page faults)"

delta_test2=$(( C_MIN[test2] - C_MIN[test0] ))
if (( delta_test2 > 0 )); then
	delta_str="+${delta_test2}"
elif (( delta_test2 == 0 )); then
	delta_str="+0"
else
	delta_str="${delta_test2}"
fi
if (( delta_test2 <= 10 )); then
	echo -e "Free quality: ${GREEN}✓  ${NC} (${delta_str} minor page faults)"
else
	echo -e "Free quality: ${RED}X  ${NC} (${delta_str} minor page faults)"
fi

# Realloc status
if (( C_REALLOC[test3] == 1 )); then
	echo -e "Realloc:      ${GREEN}✓${NC}"
else
	echo -e "Realloc:      ${RED}X${NC}"
fi

# Alignment test (test5)
unset LD_PRELOAD
unset MALLOC_CHECK_
export LD_LIBRARY_PATH="${SCRIPT_DIR}/../lib:$LD_LIBRARY_PATH"
export LD_PRELOAD="libft_malloc.so"
"${SCRIPT_DIR}/evaluation/test5" >/dev/null 2>&1
align_exit=$?
unset LD_PRELOAD
unset MALLOC_CHECK_
if (( align_exit == 0 )); then
	echo -e "Alignment:    ${GREEN}✓${NC}"
else
	echo -e "Alignment:    ${RED}X${NC}"
fi

echo
echo "Press any key to continue..."
read -n 1 -s
tput cuu1
tput el

# Show Alloc Mem
echo -e "${CYAN}SHOW_ALLOC_MEM${NC}"
echo -e "${CYAN}--------------${NC}"
echo

"${SCRIPT_DIR}/evaluation/test4"

echo
echo "Press any key to continue..."
read -n 1 -s
tput cuu1
tput el

# Show Alloc Mem EX
echo -e "${CYAN}SHOW_ALLOC_MEM_EX${NC}"
echo -e "${CYAN}-----------------${NC}"
echo

"${SCRIPT_DIR}/evaluation/test6"

echo
echo "Press any key to continue..."
read -n 1 -s
tput cuu1
tput el

# Show Alloc Mem EX
echo -e "${CYAN}SHOW_ALLOC_HISTORY${NC}"
echo -e "${CYAN}------------------${NC}"
echo

"${SCRIPT_DIR}/evaluation/test7"
echo

rm -f "${SCRIPT_DIR}/evaluation/test0" "${SCRIPT_DIR}/evaluation/test1" "${SCRIPT_DIR}/evaluation/test2" "${SCRIPT_DIR}/evaluation/test3" "${SCRIPT_DIR}/evaluation/test4" "${SCRIPT_DIR}/evaluation/test5" "${SCRIPT_DIR}/evaluation/test6" "${SCRIPT_DIR}/evaluation/test7"