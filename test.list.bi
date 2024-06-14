:i count 1
:b shell 35
./simpleC -d com -e "int main() {}"
:i returncode 0
:b stdout 33
ASSEMBLY:
GLOBAL main
main: RET 

:b stderr 0

