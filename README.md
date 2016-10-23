# kinshi
osu! beatmap difficulty graphing. Based on the super cool, oppai difficulty calculator.

While oppai is awesome for figuring out how much pp a map may give it doesn't
tell you anything how the difficulty varies throughout the song. You can't tell if it's
pp gold mine with only one difficult section or a pp trap with multiple crazy streams or jumps.

With kinshi you can see the actually difficulty rating used in the pp calculation graphed
over the length of the song. Now you can see visually just how many 300pp sections that song really
has before you try and FC it!.

# Dependencies
kinshi depends on libzip which compiles easily on windows with mingw and should be available in 
your standard package manager if you are on windows.

In order to run the gnuplot script you will also need gnuplot which can be downloaded at the [gnuplot
website](http://www.gnuplot.info/download.html), or if you are using linux it should also be
available in your standard package manager.

And thats it!

# Usage
Once you've got everything compiled you can use kinshi to generate all the raw and windowed strain values
used in aim and speed difficulty calculation on stdout. You may want to pipe these to a file to use with
gnuplot or your favorite graphing tool.

Example usage
```
kinshi test.osz
```

Example output
````
2133.000000	1.000000	1.000000
2673.000000	5.645927	5.297798
2808.000000	25.517988	23.606364
2943.000000	41.004492	39.522267
3078.000000	53.506342	54.794305
3213.000000	62.248165	64.605231
...


400	0.000000	0.000000
800	0.000000	0.000000
1200	0.000000	0.000000
1600	0.000000	0.000000
2000	0.000000	0.000000
2400	1.000000	1.000000
2800	5.645927	5.297798
3200	53.506342	54.794305
3600	62.248165	64.605231
...
```
The first section of output is the timing of the hit object in ms followed by its aim strain and its speed strain.
In the second section we have the map broken up into 400ms windows, with the end of the window in ms followed
by the aim strain for that window and the speed strain for that window.

An example gnuplot script for plotting this data (and the gnuplot script used to create the compare_report.svg) is
available in `plot_data.gp`.

# Comparison to oppai data
I have went ahead and plotted the output of kinshi's data against the data used in oppai. You can see the report here
![kinshi oppai comparison](https://cdn.rawgit.com/kkartaltepe/kinshi/master/compare_report.svg)

Notable differences here are that oppai sets the strain on all spinners to 0, meanwhile kinshi treats them as circles.
This causes a slight rise in the raw strains after a spinner and can lead to some interesting smoothing artifacts
where spinners are the last hit object of the map.
You can see the data in kinshi spikes dramatically meanwhile oppai's falls. This is because in `Freedom Dive [another]`,
the last hit object is a spinner and is set to 0 causing oppai to not even consider that window or use the previous
hit objects strain scaled down to when the spinner starts. In kinshi it may lead to the window containing the final
spinner having a slightly higher strain or having an additional window(s) containing the spinner.
