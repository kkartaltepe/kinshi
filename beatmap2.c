
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

#include <zip.h> // libzip from https://nih.at/libzip relies on zlib


#define MAX_OSU_FILE_NUM 64
#define MAX_HIT_OBJS 4096


#define HIT_TYPE_CIRCLE 1
#define HIT_TYPE_SLIDER 2
#define HIT_TYPE_SPINNER 8

typedef struct hit_obj {
	double x,y,time; // Double for calculations
	int type,hitsound;
	// Slider only fields
	int slider_type;
	int repeats,length;
} hit_obj_t;

typedef struct osz_data {
	zip_t* zip;

	size_t maps_num;
	uint64_t maps[MAX_OSU_FILE_NUM];
} osz_data_t;

void free_osz_data(osz_data_t* osz) {
	zip_close(osz->zip);
}

typedef struct beatmap {
	char* name;

	double AR,OD,CS,HP,SV,STR;

	size_t hit_objs_num;
	hit_obj_t* hit_objs;
} beatmap_t;

void free_beatmap_data(beatmap_t* b) {
	if(b) {
		if(b->hit_objs_num >= 0)
			free(b->hit_objs);
		if(b->name)
			free(b->name);
	}
}

const double circlesize_buff_treshold = 30;
const double playfield_width = 512.0;
double distance_normalizer(beatmap_t* b) {
	double circle_radius = (playfield_width / 16.f) * (1.f - 0.7f / 5.f *(b->CS - 5.f));

	// positions are normalized on circle radius so that we can calc as
	// if everything was the same circlesize
	double distance_scale = 52.0f / circle_radius;

	// cs buff (credits to osuElements, I have confirmed that this is
	// indeed accurate)
	if (circle_radius < circlesize_buff_treshold) {
		distance_scale *= 1.f + fmin((circlesize_buff_treshold - circle_radius), 5.f) / 50.f;
	}

	return distance_scale;
}

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

uint64_t zip_find_by_extension(zip_t* zip, uint64_t* ind_buf, size_t buff_size, const char* ext) {
	uint64_t num_files;
	if((num_files = zip_get_num_entries(zip, 0)) < 0) {
		// fprintf(stderr, "Failed to get the number of files in %s\n", osz);
		return -1;
	}
	// printf("Found osz with %"PRIu64" files\n", num_files);

	uint64_t matches = 0;
	for(uint64_t i = 0; i < num_files; i++) { //Look for .osu files.
		const char* fname = zip_get_name(zip, i, ZIP_FL_ENC_GUESS);
		if(fname == NULL) {
			fprintf(stderr, "Failed to get name of file %"PRIu64": %s\n", i, zip_strerror(zip));
		}

		size_t len = strlen(fname); // No size in zip_stat either.
		size_t ext_len = strlen(ext);
		if(strcmp(fname+(len-ext_len), ext) == 0) { // Compare the last exentension's worth of chars with the extension
			if( matches >= buff_size ) {
				fprintf(stderr, "Ran out of space to store file matches at %"PRIu64"\n", (uint64_t)buff_size); // %zu/%Iu not liking me
				return -1;
			}
			*ind_buf = i; //Add index to list of osu files
			ind_buf++;
			matches++;
		}
	}
	return matches;
}

int parse_beatmap_data(char* file_buf, uint64_t file_size, beatmap_t* beatmap) {
	const char* cursor = file_buf;

	int version = -1;
	if(sscanf(cursor, "osu file format v%i", &version) != 1) {
		fprintf(stderr, "Malformed osu file: failed to find version.\n");
		return -1;
	}

	if(version != 14) {
		printf("WARN: Osu file format v%i not guarenteed to parse correctly.", version);
	}

	// Simple linear format, no need for fancy parsers.
	// Neat parser for ini style files https://github.com/compuphase/minIni
	while(cursor >= file_buf && cursor < file_buf+file_size-1) {
		if(*(cursor+1) == '[') { // Begin section header
			char section[128];
			if(sscanf(cursor+1, "[%127[a-zA-Z0-9]]", section) == 1) {
				cursor = strchr(cursor+1, *"\n"); // Next line

				if(strcmp(section, "HitObjects") == 0) { // SECTION: HitObjects
					beatmap->hit_objs = malloc(sizeof(hit_obj_t)*MAX_HIT_OBJS);
					beatmap->hit_objs_num = 0;

					double x,y,time;
					int type,hs,bytes_read=0;
					char stype;
					// sscanf(cursor+1, "%lf,%lf,%lf,%i,%i,%c%n", &x, &y, &time, &combo, &hs, &stype, &bytes_read);
					// printf("%.20s consumes %i\n", cursor+1, bytes_read);
					while(cursor != NULL && cursor < file_buf+file_size-1
					      && sscanf(cursor+1, "%lf,%lf,%lf,%i,%i,%c%n", &x, &y, &time, &type, &hs, &stype, &bytes_read) == 6) {
						if(beatmap->hit_objs_num < MAX_HIT_OBJS) {
							if(type & HIT_TYPE_SLIDER) {
								cursor+=bytes_read+1;
								int side_effect;
								while(cursor != NULL && cursor < file_buf+file_size-1
								      && sscanf(cursor, "|%i:%*i%n", &side_effect, &bytes_read) == 1) {
									// Chomp through slider points
									cursor += bytes_read;
								}

								double repeats=0,length=1;
								if(sscanf(cursor, ",%lf,%lf", &repeats, &length) != 2) {
									// Just keep going if we cant parse this
									fprintf(stderr, "Failed to parse repeats and length for slider(%i)\n", type);
								}

								beatmap->hit_objs[beatmap->hit_objs_num] = (hit_obj_t){x,y,time,HIT_TYPE_SLIDER,hs,stype,repeats,length};
							} else if (type & HIT_TYPE_CIRCLE) {
								beatmap->hit_objs[beatmap->hit_objs_num] = (hit_obj_t){x,y,time,HIT_TYPE_CIRCLE,hs,0,0,0};
							} else if (type & HIT_TYPE_SPINNER) {
								beatmap->hit_objs[beatmap->hit_objs_num] = (hit_obj_t){x,y,time,HIT_TYPE_SPINNER,hs,0,0,0};
							} else {
								fprintf(stderr, "Uknown hit object detected, skipping it.\n");
								continue;
							}

							beatmap->hit_objs_num++;
						} // Silently dont store hit objects past MAX_HIT_OBJS

						cursor = strchr(cursor+1, *"\n");
					} // No more hit objects, fall out to section parser.

					// printf("Parsed %lu hit objects\n", (long)beatmap->hit_objs_num);
				} else if(strcmp(section, "Difficulty") == 0) { // SECTION: Difficulty
					char name[128];
					double value;
					while(cursor != NULL && cursor < file_buf+file_size-1
					      && sscanf(cursor+1, "%127[a-zA-Z]:%lf", name, &value) == 2) {
						if(strcmp(name, "ApproachRate") == 0) {
							beatmap->AR = value;
						} else if(strcmp(name, "OverallDifficulty") == 0) {
							beatmap->OD = value;
						} else if(strcmp(name, "CircleSize") == 0) {
							beatmap->CS = value;
						} else if(strcmp(name, "HPDrainRate") == 0) {
							beatmap->HP = value;
						} else if(strcmp(name, "SliderMultiplier") == 0) {
							beatmap->SV = value;
						} else if(strcmp(name, "SliderTickRate") == 0) {
							beatmap->STR = value;
						} else {
							fprintf(stderr, "WARN: Unhandled Difficulty setting %s\n", name);
						}

						cursor = strchr(cursor+1, *"\n");
					}

					// printf("Parsed beatmap difficulty ar:%lf, od:%lf, cs:%lf, hp:%lf\n", beatmap->AR, beatmap->OD, beatmap->CS, beatmap->HP);
				} else {
					// fprintf(stderr, "WARN: Unhandled section %s\n", section);
				}

			} else {
				fprintf(stderr, "WARN:Failed to parse section header\n");
			}

		}
		
		// Done parsing sections, progress cursor to next line
		cursor = strchr(cursor+1, *"\n"); // Also simple newline delimited format guarenteed no newlines in data.
	}

	return 0;
}

int parse_beatmap(osz_data_t* osz, uint64_t beatmap_num, beatmap_t* beatmap) {
	if(osz == NULL) {
		fprintf(stderr, "parse_beatmap called with invalid osz data\n");
	}

	if(beatmap_num >= osz->maps_num || beatmap_num < 0) {
		fprintf(stderr, "Invalid beatmap number %"PRIu64" must be between 0 and %"PRIu64"\n", beatmap_num, osz->maps_num-1);
		return -1;
	}

	zip_stat_t stat = {0};
	if(zip_stat_index(osz->zip, osz->maps[beatmap_num], 0, &stat) != 0) {
		fprintf(stderr, "Failed to stat osu file at index %"PRIu64": %s\n", osz->maps[beatmap_num], zip_strerror(osz->zip));
		return -1;
	}

	char* file_buf = NULL;
	if((file_buf = malloc(stat.size)) == NULL) {
		fprintf(stderr, "Failed to allocate %"PRIu64" bytes for .osu file\n", stat.size);
		return -1;
	}

	zip_file_t* zip_file;
	if((zip_file = zip_fopen_index(osz->zip, osz->maps[beatmap_num], 0)) == NULL) {
		fprintf(stderr, "Failed to read file '%s': %s\n", stat.name, zip_strerror(osz->zip));
		return -1;
	}
	
	zip_fread(zip_file, file_buf, stat.size); // Slurp it up.
	
	// printf("Parsing beatmap '%s'\n", stat.name);
	parse_beatmap_data(file_buf, stat.size, beatmap);

	free(file_buf);
	zip_fclose(zip_file);

	return 0;
}

int parse_osz(const char* osz, osz_data_t* osz_data) {
	int err = 0;
	zip_t* zip = NULL;

	if((zip = zip_open(osz, ZIP_RDONLY, &err)) == NULL) {
		zip_error_t zerr = {0};
		zip_error_init_with_code(&zerr, err);
		fprintf(stderr, "Failed to unzip %s: error %s\n", osz, zip_error_strerror(&zerr));
		return -1;
	}

	osz_data->maps_num = zip_find_by_extension(zip, osz_data->maps, MAX_OSU_FILE_NUM, ".osu");
	osz_data->zip = zip;

	return 0;
}

int main(int argc, char** argv) {
	if(argc-1 != 1) {
		fprintf(stderr, "Please reference exactly 1 osz file to parse\n");
		return -1;
	}
	osz_data_t osz = {0};
	beatmap_t b = {0};

	parse_osz(argv[1], &osz);
	parse_beatmap(&osz, 0, &b);

	double* strains = malloc(sizeof(double)*2*b.hit_objs_num);
	memset(strains, 1, sizeof(double)*2*b.hit_objs_num);
	double dist_normalizer = distance_normalizer(&b);
	int circles=0, sliders=0, spinners=0;
	for(int i = 1; i < b.hit_objs_num; ++i) {
		double time_elapsed = b.hit_objs[i].time - b.hit_objs[i-1].time;
		double normalized_dist = sqrt(pow(b.hit_objs[i].x-b.hit_objs[i-1].x,2)+pow(b.hit_objs[i].y-b.hit_objs[i-1].y,2))*dist_normalizer;
		
		double prev_aim_strain = strains[(i-1)*2];
		strains[i*2] = aim_strain(prev_aim_strain, time_elapsed, normalized_dist);
		double prev_speed_strain = strains[(i-1)*2+1];
		strains[i*2+1] = speed_strain(prev_speed_strain, time_elapsed, normalized_dist);

		if(b.hit_objs[i].type == HIT_TYPE_CIRCLE) {
			circles++;
		} else if(b.hit_objs[i].type == HIT_TYPE_SLIDER) {
			sliders++;
		} else {
			spinners++;
		}
	}
	// printf("Map has %i circles, %i sliders, %i spinners\n", circles, sliders, spinners);
	for(int i = 0; i < b.hit_objs_num; i++) {
		printf("%lf\t%lf\t%lf\n", b.hit_objs[i].time, strains[i*2], strains[i*2+1]);
	}
	printf("\n\n");
	fflush(stdout);

	if (b.hit_objs_num > 0) {
		int window_size = 400; // 400ms
		int window_pos = b.hit_objs[0].time;
		int window_num = 0;
		int num_windows = (int)ceil((b.hit_objs[b.hit_objs_num-1].time - b.hit_objs[0].time)/window_size);
		double* window_maxes = malloc(sizeof(double)*2*num_windows);
		memset(window_maxes, 0, sizeof(double)*2*num_windows);
		if(window_maxes == NULL) { fprintf(stderr, "Failed to allocate bytes for window %i strains\n", num_windows); fflush(stderr); }
		window_maxes[0] = 0;
		window_maxes[0] = 0;
		for(int i = 1; i < b.hit_objs_num; ++i) { // Windowing function to set max strain per window and decay previous max strain over empty windows.
			while(window_pos+window_size < b.hit_objs[i].time) { //Next object was outside our window!
				window_maxes[window_num*2] = strains[(i-1)*2] * pow(aim_decay, (window_pos+window_size - b.hit_objs[i-1].time) / 1000.0);
				window_maxes[window_num*2+1] = strains[(i-1)*2+1] * pow(speed_decay, (window_pos+window_size - b.hit_objs[i-1].time) / 1000.0);
				
				window_pos += window_size;
				window_num++;
			}

			window_maxes[window_num*2] = fmax(window_maxes[window_num*2], strains[(i-1)*2]);
			window_maxes[window_num*2+1] = fmax(window_maxes[window_num*2+1], strains[(i-1)*2+1]);
		}

		for(int i = 0; i < window_num; i++) {
			printf("%lf\t%lf\t%lf\n", i*400.0+b.hit_objs[0].time, window_maxes[i*2+1], window_maxes[i*2]);
		}
	}


	


	free(strains);
	free_osz_data(&osz);
	free_beatmap_data(&b);
}