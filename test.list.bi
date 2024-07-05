:i count 34
:b shell 22
./simpleC -D par -e ""
:i returncode 0
:b stdout 9
AST:
NULL
:b stderr 0

:b shell 35
./simpleC -D com -e "int main() {}"
:i returncode 0
:b stdout 57
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

main: RET 

:b stderr 0

:b shell 48
./simpleC -D com -e "int add() {} int main() {}"
:i returncode 0
:b stdout 67
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

add: RET 
main: RET 

:b stderr 0

:b shell 46
./simpleC -D com -e "int main() { return 1; }"
:i returncode 0
:b stdout 69
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

main: RAM_AL 0x01 RET 

:b stderr 0

:b shell 45
./simpleC -D com -e "void main() { return; }"
:i returncode 0
:b stdout 57
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

main: RET 

:b stderr 0

:b shell 57
./simpleC -D com -e "int main() { int a = 0; return a; }"
:i returncode 0
:b stdout 111
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

main: RAM_AL 0x00 PUSHA SP_A RAM_BL 0x02 SUM A_B rB_A INCSP RET 

:b stderr 0

:b shell 47
./simpleC -D com -e "int main() { int a = 0; }"
:i returncode 0
:b stdout 81
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

main: RAM_AL 0x00 PUSHA INCSP RET 

:b stderr 0

:b shell 54
./simpleC -D com -e "int main() { int a = 0; a = 2; }"
:i returncode 0
:b stdout 105
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

main: RAM_AL 0x00 PUSHA RAM_AL 0x02 INCSP PUSHA INCSP RET 

:b stderr 0

:b shell 52
./simpleC -D com -e "int main() { int *b; *b = 1; }"
:i returncode 0
:b stdout 120
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

main: PUSHA SP_A RAM_BL 0x02 SUM A_B rB_A A_B RAM_AL 0x01 A_rB INCSP RET 

:b stderr 0

:b shell 55
./simpleC -D com -e "int *main() { int *b; return b; }"
:i returncode 0
:b stdout 99
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

main: PUSHA SP_A RAM_BL 0x02 SUM A_B rB_A INCSP RET 

:b stderr 0

:b shell 42
./simpleC -D com -e "int main() { -2+1; }"
:i returncode 0
:b stdout 105
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

main: RAM_AL 0x02 RAM_BL 0x00 SUB A_B RAM_AL 0x01 SUM RET 

:b stderr 0

:b shell 44
./simpleC -D typ -e "int main() { -2*1/2; }"
:i returncode 0
:b stdout 176
TYPED AST:
GLOBAL(FUNCDECL(INT main NULL BLOCK(BINARYOP(/ BINARYOP(* UNARYOP(- FAC(2) {INT}) {INT} FAC(1) {INT}) {INT} FAC(2) {INT}) {INT} NULL) {VOID}) {FUNC INT} NULL) {VOID}
:b stderr 0

:b shell 55
./simpleC -D com -e "int *main() { int a; return &a; }"
:i returncode 0
:b stdout 90
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

main: PUSHA SP_A RAM_BL 0x02 SUM INCSP RET 

:b stderr 0

:b shell 55
./simpleC -D com -e "int main() { int *a; return *a; }"
:i returncode 0
:b stdout 108
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

main: PUSHA SP_A RAM_BL 0x02 SUM A_B rB_A A_B rB_A INCSP RET 

:b stderr 0

:b shell 56
./simpleC -D com -e "int main() { char *a; return *a; }"
:i returncode 0
:b stdout 109
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

main: PUSHA SP_A RAM_BL 0x02 SUM A_B rB_A A_B rB_AL INCSP RET 

:b stderr 0

:b shell 44
./simpleC -D com -e "int main() { main(); }"
:i returncode 0
:b stdout 69
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

main: CALLR $main RET 

:b stderr 0

:b shell 63
./simpleC -D com -e "int main(int a, char b) { main(2,0x02); }"
:i returncode 0
:b stdout 117
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

main: RAM_AL 0x02 PUSHA RAM_AL 0x02 PUSHA CALLR $main INCSP INCSP RET 

:b stderr 0

:b shell 81
./simpleC -D com -e "int add(int a) { return a; } int main() { return add(10); }"
:i returncode 0
:b stdout 132
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

add: SP_A RAM_BL 0x04 SUM A_B rB_A RET 
main: RAM_AL 0x0A PUSHA CALLR $add INCSP RET 

:b stderr 0

:b shell 45
./simpleC -D typ -e "int main() { 2*(1+3); }"
:i returncode 0
:b stdout 173
TYPED AST:
GLOBAL(FUNCDECL(INT main NULL BLOCK(BINARYOP(* FAC(2) {INT} GROUP((BINARYOP(+ FAC(1) {INT} FAC(3) {INT}) {INT}) {INT}) {INT} NULL) {VOID}) {FUNC INT} NULL) {VOID}
:b stderr 0

:b shell 55
./simpleC -D com -e "int main() { char *a = \"asd\"; }"
:i returncode 0
:b stdout 98
ASSEMBLY:
GLOBAL _start
_000: "asd" 0x00 
_start: 
	JMPR $main

main: RAM_A _000 PUSHA INCSP RET 

:b stderr 0

:b shell 66
./simpleC -D com -e "int main() { char *a = \"asd\"; *a = 0x10; }"
:i returncode 0
:b stdout 150
ASSEMBLY:
GLOBAL _start
_000: "asd" 0x00 
_start: 
	JMPR $main

main: RAM_A _000 PUSHA SP_A RAM_BL 0x02 SUM A_B rB_A A_B RAM_AL 0x10 AL_rB INCSP RET 

:b stderr 0

:b shell 73
./simpleC -D com -e "char main() { char *a = \"asd\"; return *(a + 2); }"
:i returncode 0
:b stdout 158
ASSEMBLY:
GLOBAL _start
_000: "asd" 0x00 
_start: 
	JMPR $main

main: RAM_A _000 PUSHA SP_A RAM_BL 0x02 SUM A_B rB_A A_B RAM_AL 0x02 SUM A_B rB_AL INCSP RET 

:b stderr 0

:b shell 71
./simpleC -D com -e "char main() { char *a = \"asd\"; *(a+1) = 0x01; }"
:i returncode 0
:b stdout 170
ASSEMBLY:
GLOBAL _start
_000: "asd" 0x00 
_start: 
	JMPR $main

main: RAM_A _000 PUSHA SP_A RAM_BL 0x02 SUM A_B rB_A A_B RAM_AL 0x01 SUM A_B RAM_AL 0x01 AL_rB INCSP RET 

:b stderr 0

:b shell 47
./simpleC -D com -e "int a = 10; int main() {}"
:i returncode 0
:b stdout 71
ASSEMBLY:
GLOBAL _start
_000: 0x000A 
_start: 
	JMPR $main

main: RET 

:b stderr 0

:b shell 42
./simpleC -D com -e "int a; int main() {}"
:i returncode 0
:b stdout 71
ASSEMBLY:
GLOBAL _start
_000: 0x0000 
_start: 
	JMPR $main

main: RET 

:b stderr 0

:b shell 53
./simpleC -D com -e "int a; int main() { return a; }"
:i returncode 0
:b stdout 91
ASSEMBLY:
GLOBAL _start
_000: 0x0000 
_start: 
	JMPR $main

main: RAM_A _000 A_B rB_A RET 

:b stderr 0

:b shell 50
./simpleC -D com -e "int a; int main() { a = 2; }"
:i returncode 0
:b stdout 103
ASSEMBLY:
GLOBAL _start
_000: 0x0000 
_start: 
	JMPR $main

main: RAM_A _000 A_B RAM_AL 0x02 A_rB RET 

:b stderr 0

:b shell 55
./simpleC -D com -e "int a; int *main() { return &a; }"
:i returncode 0
:b stdout 82
ASSEMBLY:
GLOBAL _start
_000: 0x0000 
_start: 
	JMPR $main

main: RAM_A _000 RET 

:b stderr 0

:b shell 54
./simpleC -D com -e "char *a = \"asd\"; int main() {}"
:i returncode 0
:b stdout 86
ASSEMBLY:
GLOBAL _start
_000: "asd" 0x00 
_start: 
	JMPR $main
RAM_A _000 
main: RET 

:b stderr 0

:b shell 57
./simpleC -D com -O1 -e "int a; int main() { return a; }"
:i returncode 0
:b stdout 87
ASSEMBLY:
GLOBAL _start
_000: 0x0000 
_start: 
	JMPR $main

main: RAM_B _000 rB_A RET 

:b stderr 0

:b shell 56
./simpleC -D com -O1 -e "int main() { int *b; *b = 1; }"
:i returncode 0
:b stdout 90
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

main: PUSHA A_B RAM_AL 0x01 A_rB INCSP RET 

:b stderr 0

:b shell 61
./simpleC -D com -O1 -e "int main() { int a = 0; return a; }"
:i returncode 0
:b stdout 81
ASSEMBLY:
GLOBAL _start
_start: 
	JMPR $main

main: RAM_AL 0x00 PUSHA INCSP RET 

:b stderr 0

:b shell 50
./simpleC -D com -e "int main() { {10, 20, 30}; }"
:i returncode 0
:b stdout 96
ASSEMBLY:
GLOBAL _start
_000: 0x000A 0x0014 0x001E 
_start: 
	JMPR $main

main: RAM_A _000 RET 

:b stderr 0

:b shell 54
./simpleC -D com -O1 -e "int main() { {10, 20, 30}; }"
:i returncode 0
:b stdout 96
ASSEMBLY:
GLOBAL _start
_000: 0x000A 0x0014 0x001E 
_start: 
	JMPR $main

main: RAM_A _000 RET 

:b stderr 0

