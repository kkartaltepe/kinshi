set terminal svg size 768,1024;
set autoscale;
set multiplot layout 2,1 title 'Oppai vs Kinshi'

set title 'Raw Strains';
set xlabel 'Song Time(ms)';
plot 'oppai.dat' i 1 using 1:2 t 'Oppai Aim' smooth bezier, \
     'kinshi.dat' i 1 u 1:2 t 'Kinshi Aim' smooth bezier, \
     'oppai.dat' i 2 using 1:2 t'Oppai Speed' smooth bezier, \
     'kinshi.dat' i 1 u 1:3 t 'Kinshi Speed' smooth bezier;

set title 'Aggregated Strains';
set xlabel 'Song Time(ms)';
plot 'oppai.dat' i 0 using 1:3 t 'Oppai Aim' smooth bezier, \
     'kinshi.dat' i 0 u 1:2 t 'Kinshi Aim' smooth bezier, \
     'oppai.dat' i 0 using 1:2 t'Oppai Speed' smooth bezier, \
     'kinshi.dat' i 0 u 1:3 t 'Kinshi Speed' smooth bezier;

unset multiplot;