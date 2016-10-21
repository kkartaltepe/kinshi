all: kinshi.exe compare_report.svg
.PHONY: all

kinshi.exe: beatmap2.c
	gcc -std=c99 -Wall beatmap2.c -lzip -g3 -o kinshi.exe

compare_report.svg: kinshi plot_data.gp diff2.dat oppai.dat
	./a.exe /d/Downloads/477725\ AK\ X\ LYNX\ ft.\ Veela\ -\ Virtual\ Paradise.osz > diff2.dat
	gnuplot -p plot_data.gp > compare_report.svg