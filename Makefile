all: kinshi.exe compare_report.svg
.PHONY: all

kinshi.exe: *.c *.h
	gcc -std=c99 -Wall main.c -lzip -g3 -o kinshi.exe

kinshi.dat: kinshi.exe test.osz
	./kinshi.exe "test.osz" "1" > kinshi.dat

compare_report.svg: kinshi plot_data.gp kinshi.dat oppai.dat
	gnuplot -p plot_data.gp > compare_report.svg