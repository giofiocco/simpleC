:i count 17
:b shell 22
./simpleC -D com -e ""
:i returncode 0
:b stdout 23
ASSEMBLY:
GLOBAL main


:b stderr 0

:b shell 35
./simpleC -D com -e "int main() {}"
:i returncode 0
:b stdout 33
ASSEMBLY:
GLOBAL main
main: RET 

:b stderr 0

:b shell 48
./simpleC -D com -e "int add() {} int main() {}"
:i returncode 0
:b stdout 43
ASSEMBLY:
GLOBAL main
add: RET 
main: RET 

:b stderr 0

:b shell 46
./simpleC -D com -e "int main() { return 1; }"
:i returncode 0
:b stdout 45
ASSEMBLY:
GLOBAL main
main: RAM_AL 0x01 RET 

:b stderr 0

:b shell 45
./simpleC -D com -e "void main() { return; }"
:i returncode 0
:b stdout 33
ASSEMBLY:
GLOBAL main
main: RET 

:b stderr 0

:b shell 57
./simpleC -D com -e "int main() { int a = 0; return a; }"
:i returncode 0
:b stdout 63
ASSEMBLY:
GLOBAL main
main: RAM_AL 0x00 PUSHA PEEKA INCSP RET 

:b stderr 0

:b shell 47
./simpleC -D com -e "int main() { int a = 0; }"
:i returncode 0
:b stdout 57
ASSEMBLY:
GLOBAL main
main: RAM_AL 0x00 PUSHA INCSP RET 

:b stderr 0

:b shell 54
./simpleC -D com -e "int main() { int a = 0; a = 2; }"
:i returncode 0
:b stdout 81
ASSEMBLY:
GLOBAL main
main: RAM_AL 0x00 PUSHA RAM_AL 0x02 INCSP PUSHA INCSP RET 

:b stderr 0

:b shell 52
./simpleC -D com -e "int main() { int *b; *b = 1; }"
:i returncode 0
:b stdout 68
ASSEMBLY:
GLOBAL main
main: DECSP PEEKB RAM_AL 0x01 A_rB INCSP RET 

:b stderr 0

:b shell 55
./simpleC -D com -e "int *main() { int *b; return b; }"
:i returncode 0
:b stdout 51
ASSEMBLY:
GLOBAL main
main: DECSP PEEKA INCSP RET 

:b stderr 0

:b shell 42
./simpleC -D com -e "int main() { -2+1; }"
:i returncode 0
:b stdout 81
ASSEMBLY:
GLOBAL main
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
:b stdout 66
ASSEMBLY:
GLOBAL main
main: DECSP SP_A RAM_BL 0x02 SUM INCSP RET 

:b stderr 0

:b shell 55
./simpleC -D com -e "int main() { int *a; return *a; }"
:i returncode 0
:b stdout 60
ASSEMBLY:
GLOBAL main
main: DECSP PEEKA A_B rB_A INCSP RET 

:b stderr 0

:b shell 44
./simpleC -D com -e "int main() { main(); }"
:i returncode 0
:b stdout 45
ASSEMBLY:
GLOBAL main
main: CALLR $main RET 

:b stderr 0

:b shell 63
./simpleC -D com -e "int main(int a, char b) { main(2,0x02); }"
:i returncode 0
:b stdout 93
ASSEMBLY:
GLOBAL main
main: RAM_AL 0x02 PUSHA RAM_AL 0x02 PUSHA CALLR $main INCSP INCSP RET 

:b stderr 0

:b shell 81
./simpleC -D com -e "int add(int a) { return a; } int main() { return add(10); }"
:i returncode 0
:b stdout 90
ASSEMBLY:
GLOBAL main
add: PEEKAR 0x04 RET 
main: RAM_AL 0x0A PUSHA CALLR $add INCSP RET 

:b stderr 0

