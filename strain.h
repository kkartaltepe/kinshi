#ifndef STRAIN_H
#define STRAIN_H
#include <math.h>
#include "beatmap.h"

// Not really a header, but using for single file compilation


const double speed_decay = 0.3; // Decay of previous strain per second
const double speed_base = 1400;
// arbitrary tresholds to determine when a stream is spaced enough that is 
// becomes hard to alternate.
const double stream_spacing = 110;
const double single_spacing = 125;
// almost the normalized circle diameter (104px)
const double almost_diameter = 90;
double speed_strain(double prev_strain, double time_elapsed, double normalized_dist) {

	double time_factor = 1.0f/fmax(50, time_elapsed);
	double decay = pow(speed_decay, time_elapsed / 1000.0);

	double dist_factor = 0.95;
	if (normalized_dist > single_spacing) {
		dist_factor = 2.5;
	}
	else if (normalized_dist > stream_spacing) {
		dist_factor = 1.6 + 0.9 *
			(normalized_dist - stream_spacing) /
			(single_spacing - stream_spacing);
	}
	else if (normalized_dist > almost_diameter) {
		dist_factor = 1.2 + 0.4 * (normalized_dist - almost_diameter)
			/ (stream_spacing - almost_diameter);
	}
	else if (normalized_dist > almost_diameter / 2.0) {
		dist_factor = 0.95 + 0.25 * 
			(normalized_dist - almost_diameter / 2.0) /
			(almost_diameter / 2.0);
	}

	double strain = speed_base*time_factor*dist_factor;
	double total_strain = prev_strain*decay+strain;
	return total_strain;
}

const double aim_decay = 0.15; // Decay of previous strain per second
const double aim_base = 26.25;
double aim_strain(double prev_strain, double time_elapsed, double normalized_dist) {
	
	double time_factor = 1.0f/fmax(50, time_elapsed);
	double decay = pow(aim_decay, time_elapsed / 1000.0);

	double dist_factor = pow(normalized_dist, 0.99);

	double strain = aim_base*time_factor*dist_factor;
	double total_strain = prev_strain*decay+strain;
	return total_strain;
}

// Number of strains should be equal to number of hit objects. Isnt checked right now.
int calc_strains(beatmap_t* b, double* aim_strains, double* speed_strains) {
	aim_strains[0] = 1; speed_strains[0] = 1; // Cant use memset on doubles... Doh
	double dist_normalizer = distance_normalizer(b);
	for(int i = 1; i < b->hit_objs_num; ++i) {
		double time_elapsed = b->hit_objs[i].time - b->hit_objs[i-1].time;
		double normalized_dist = sqrt(pow(b->hit_objs[i].x-b->hit_objs[i-1].x,2)+pow(b->hit_objs[i].y-b->hit_objs[i-1].y,2))*dist_normalizer;
		
		double prev_aim_strain = aim_strains[i-1];
		aim_strains[i] = aim_strain(prev_aim_strain, time_elapsed, normalized_dist);
		double prev_speed_strain = speed_strains[i-1];
		speed_strains[i] = speed_strain(prev_speed_strain, time_elapsed, normalized_dist);
	}

	return 0;
}

// Number of windows function of beatmap time, window_size, window_start, but isnt checked right now.
int calc_agg_strains(beatmap_t* b, double* aim_strains, double* speed_strains, double* window_aim_strains, double* window_speed_strains, int window_size, int window_start) {
	int window_num = 0;
	double decayed_prev_strain_aim = 0, decayed_prev_strain_speed = 0;

	for(int i = 0; i < b->hit_objs_num; i++) { // Windowing function to set max strain per window and decay previous max strain over empty windows.
		while(window_start+window_size < b->hit_objs[i].time) { //Next object was outside our window!
			if ( i > 0) {
				decayed_prev_strain_aim = aim_strains[i-1] * pow(aim_decay, (window_start+window_size - b->hit_objs[i-1].time) / 1000.0);
				decayed_prev_strain_speed = speed_strains[i-1] * pow(speed_decay, (window_start+window_size - b->hit_objs[i-1].time) / 1000.0);
			}
			
			window_start += window_size;
			window_num++;
			window_aim_strains[window_num] = decayed_prev_strain_aim;
			window_speed_strains[window_num] = decayed_prev_strain_speed;
		}
		window_aim_strains[window_num] = fmax(window_aim_strains[window_num], aim_strains[i]);
		window_speed_strains[window_num] = fmax(window_speed_strains[window_num], speed_strains[i]);
	}

	return 0;
}

#endif