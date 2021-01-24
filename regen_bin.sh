#!/bin/bash
echo "regenerating hello.bin"
arm-none-eabi-as -o hello.o hello.s 
arm-none-eabi-ld -o hello.elf hello.o
arm-none-eabi-objdump -D hello.elf > hello.list
arm-none-eabi-objcopy -O binary hello.elf hello.bin
