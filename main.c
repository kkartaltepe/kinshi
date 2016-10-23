
#include "beatmap.h"
#include "strain.h" 
// Not really, headers just split to break things up, still compiling single file.

int main(int argc, char** argv) {
	if(argc > 3 || argc < 2) {
		fprintf(stderr, "Usage: ./kinshi osz_path [beatmap_num]\n");
		return 1;
	}
	osz_data_t osz = {0};
	beatmap_t b = {0};

	parse_osz(argv[1], &osz);
	if(argc == 2) { // No beatmap_num specified, list beatmaps.
		for(int i = 0; i < osz.maps_num; i++) {
			fflush(stdout); fflush(stdout);
			parse_beatmap(&osz, i, &b);
			printf("%d) %s [%s](mode:%d)\n", i, b.name, b.version, b.mode);
			free_beatmap_data(&b);
		}
		return 0;
	} // Otherwise actually calculate difficulties for specified map.

	int selection = 0;
	if (sscanf(argv[2], "%i", &selection) != 1) {
		printf("Please specify the number of the beatmap you want to parse\n");
		return 1;
	}

	parse_beatmap(&osz, selection, &b);

	if(b.hit_objs_num < 1) {
		fprintf(stderr, "Error: beatmap has no HitObjects\n");
		return 1;
	}

	if(b.mode != 0) {
		fprintf(stderr, "Error: kinshi only parses osu mode maps\n");
		return 1;
	}

	double* aim_strains = malloc(sizeof(double)*2*b.hit_objs_num);  // Was using the strains interleaved, legacy double sized malloc. Should be two mallocs probably.
	double* speed_strains = aim_strains+b.hit_objs_num;
	memset(aim_strains, 0, sizeof(double)*2*b.hit_objs_num);
	calc_strains(&b, aim_strains, speed_strains);
	
	for(int i = 0; i < b.hit_objs_num; i++) {
		printf("%lf\t%lf\t%lf\n", b.hit_objs[i].time, aim_strains[i], speed_strains[i]);
	}
	printf("\n\n");
	fflush(stdout);

	int window_size = 400; // 400ms
	int window_start =  0; // floor(b.hit_objs[0].time) - ((int)floor(b.hit_objs[0].time) % 400);
	int num_windows = (int)ceil((b.hit_objs[b.hit_objs_num-1].time - window_start)/window_size);
	double* window_aim_strains = malloc(sizeof(double)*2*num_windows);
	double* window_speed_strains = window_aim_strains+b.hit_objs_num;
	if(window_aim_strains == NULL) { fprintf(stderr, "Failed to allocate bytes for window %i strains\n", num_windows); fflush(stderr); }
	memset(window_aim_strains, 0, sizeof(double)*2*num_windows);

	calc_agg_strains(&b, aim_strains, speed_strains, window_aim_strains, window_speed_strains, window_size, window_start);
	printf("finished paring strains \n"); fflush(stdout);

	for(int i = 0; i < num_windows; i++) {
		printf("%i\t%lf\t%lf\n", i*window_size+window_start+window_size, window_aim_strains[i], window_speed_strains[i]); // print them by the window end
	}

	free(aim_strains);
	free(window_aim_strains);
	free_osz_data(&osz);
	free_beatmap_data(&b);
}