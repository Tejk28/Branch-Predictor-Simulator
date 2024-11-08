#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include<fcntl.h>

#include "sim_bp.h"

char cmd_text[MAX_CMDTEXT_LEN];
char sim_type[MAX_SIMTYPE_LEN];
char trace_file[MAX_FILENAME_LEN];
FILE *fp_trace=0;
char trace_str[MAX_TRACESTR_LEN];

int M1=0;
int M2=0;
int N=0;
int K=0;

pc_t pc;

predictor_t predictor;
branch_predictor bp;

void print_usage();
int validate_and_set_params(int, char *[]);
void print_params();
void print_pred_table();
void print_hybrid_params();
void print_hybrid_pred_table();

int main(int argc, char *argv[])
{

	predictor.params.num_predictions = 0;
	predictor.params.num_mispredictions = 0;

	bp.params.num_predictions = 0;
	bp.params.num_mispredictions = 0;

	if (!(argc == 4 || argc == 5 || argc == 7)) {
		printf("Error: Wrong number of inputs:%d\n", argc-1);
        exit(EXIT_FAILURE);
	}

	if (validate_and_set_params(argc, argv)) {
		printf("Error\n");
        exit(EXIT_FAILURE);
	}

	fp_trace = fopen(trace_file, "r");
	if (fp_trace == NULL) {
		// Throw error and exit if fopen() failed
        printf("Error: Unable to open file %s\n", trace_file);
        exit(EXIT_FAILURE);
	}

	if (strcmp(sim_type, "hybrid") != 0) {
		initialize_pred_params(&predictor);
		allocate_and_init_pred_tab(&predictor);
#ifdef DEBUG_FLAG
	print_pred_table();
#endif
	} else if (strcmp(sim_type, "hybrid") == 0) {
		init_hybrid_pred_params(&bp);
		allocate_and_init_hybrid_pred_tab(&bp);
	}

	while (fgets(trace_str, MAX_TRACESTR_LEN, fp_trace)) {

		sscanf(trace_str, "%x %c\n", &pc.addr, &pc.branch_outcome);

#ifdef DEBUG_FLAG
		printf("%x %c\n", pc.addr, pc.branch_outcome);
#endif

		if (strcmp(sim_type, "hybrid") != 0) {

			handle_branch_prediction(&predictor, &pc);
			predictor.params.num_predictions += 1;

		} else {

			handle_hybrid_branch_prediction(&bp, &pc);
			bp.params.num_predictions += 1;

		}
	}

	if (strcmp(sim_type, "hybrid") != 0) {
		print_params();
	} else {
		print_hybrid_params();
	}

	return 0;
}

void print_usage()
{
	printf("The Branch Predictor Simulator can be run in one of the following configurations -> \n\n");
	printf("1) Bimodal Branch Predictor Simulator:\n");
	printf("$ ./sim bimodal <M2> <tracefile>\n");
	printf("Where,\n");
	printf("M2: Number of PC bits used to index the Bimodal Prediction Table\n\n");

	printf("2) GShare Branch Predictor Simulator:\n");
	printf("$ ./sim gshare <M1> <N> <tracefile>\n");
	printf("Where,\n");
	printf("M1: Number of PC bits used to index the GShare Prediction Table\n");
	printf("N: Number of Branch History Register (BHR) bits used to index the GShare Prediction Table\n\n");


	printf("3) Hybrid Branch Predictor Simulator:\n");
	printf("$ ./sim gshare <K> <M1> <N> <M2> <tracefile>\n");
	printf("Where,\n");
	printf("K: Number of PC bits used to index the Chooser Table\n");
	printf("M1: Number of PC bits used to index the GShare Prediction Table\n");
	printf("N: Number of Branch History Register (BHR) bits used to index the GShare Prediction Table\n");
	printf("M2: Number of PC bits used to index the Bimodal Prediction Table\n\n");
}

int validate_and_set_params(int num_params, char *params[])
{
	strcpy(cmd_text, params[0]);

	strcpy(sim_type, params[1]);

	if (!(strcmp(sim_type, "bimodal") == 0 || strcmp(sim_type, "gshare") == 0 || strcmp(sim_type, "hybrid") == 0)) {
		printf("The first parameter has to be the type of branch predictor simulator! \n");
		printf("Supported types are - bimodal, gshare or hybrid\n");
		return FALSE;
	}

	if (strcmp(sim_type, "bimodal") == 0 && num_params != 4) {
   	printf("Error: %s wrong number of inputs:%d\n", sim_type, num_params-1);
    exit(EXIT_FAILURE);
		print_usage();
		return FALSE;
	} else if (strcmp(sim_type, "bimodal") == 0 && num_params == 4) {
		M2 = atoi(params[2]);
		strcpy(trace_file, params[3]);
		return TRUE;
	}

	if (strcmp(sim_type, "gshare") == 0 && num_params != 5) {
			printf("Error: %s wrong number of inputs:%d\n", sim_type, num_params-1);
    exit(EXIT_FAILURE);
		print_usage();
		return FALSE;
	} else if (strcmp(sim_type, "gshare") == 0 && num_params == 5) {
		M1 = atoi(params[2]);
		N = atoi(params[3]);
		strcpy(trace_file, params[4]);
		return TRUE;
	}

	if (strcmp(sim_type, "hybrid") == 0 && num_params != 7) {
			printf("Error: %s wrong number of inputs:%d\n", sim_type, num_params-1);
    exit(EXIT_FAILURE);
		print_usage();
		return FALSE;
	} else if (strcmp(sim_type, "hybrid") == 0 && num_params == 7) {
		K = atoi(params[2]);
		M1 = atoi(params[3]);
		N = atoi(params[4]);
		M2 = atoi(params[5]);
		strcpy(trace_file, params[6]);
		return TRUE;
	}

	return FALSE;
}

void print_params()
{
	printf("COMMAND\n");
	if (strcmp(sim_type, "bimodal") == 0) {
		printf(" %s %s %d %s\n", cmd_text, sim_type, M2, trace_file);
	} else if (strcmp(sim_type, "gshare") == 0) {
		printf(" %s %s %d %d %s\n", cmd_text, sim_type, M1, N, trace_file);
	}

	predictor.params.misprediction_rate = (float)predictor.params.num_mispredictions / (float)predictor.params.num_predictions;
	predictor.params.misprediction_rate = predictor.params.misprediction_rate * 100;

	printf("OUTPUT\n");
	printf(" number of predictions:    %ld\n", predictor.params.num_predictions);
	printf(" number of mispredictions: %ld\n", predictor.params.num_mispredictions);
	printf(" misprediction rate:       %.2f%%\n", predictor.params.misprediction_rate);

	if (strcmp(sim_type, "bimodal") == 0) {
		printf("FINAL BIMODAL CONTENTS\n");
	} else if (strcmp(sim_type, "gshare") == 0) {
		printf("FINAL GSHARE CONTENTS\n");
	}
	print_pred_table();
}

void print_pred_table()
{
	int base, offset, byte, count;
	int i;

	for (i=0 ; i < predictor.params.num_entries ; i++) {
		base = i / predictor.params.entries_per_byte;
		offset = i % predictor.params.entries_per_byte;
		offset = offset * 2;
		byte = predictor.pred_table.table[base];
		count = get_bits(byte, offset, offset + predictor.params.bits_per_entry - 1);
		printf(" %d\t%d\n", i, count);
	}
}

void print_hybrid_params()
{
	printf("COMMAND\n");
	printf(" %s %s %d %d %d %d %s\n", cmd_text, sim_type, K, M1, N, M2, trace_file);

	bp.params.misprediction_rate = (float)bp.params.num_mispredictions / (float)bp.params.num_predictions;
	bp.params.misprediction_rate = bp.params.misprediction_rate * 100;

	printf("OUTPUT\n");
	printf(" number of predictions:    %ld\n", bp.params.num_predictions);
	printf(" number of mispredictions: %ld\n", bp.params.num_mispredictions);
	printf(" misprediction rate:       %.2f%%\n", bp.params.misprediction_rate);

	print_hybrid_pred_table();
}

void print_hybrid_pred_table()
{
	int base, offset, byte, count;
	int i;

	printf("FINAL CHOOSER CONTENTS\n");
	for (i=0 ; i < bp.params.num_entries ; i++) {
		base = i / bp.params.entries_per_byte;
		offset = i % bp.params.entries_per_byte;
		offset = offset * 2;
		byte = bp.chooser_table.table[base];
		count = get_bits(byte, offset, offset + bp.params.bits_per_entry - 1);
		printf(" %d\t%d\n", i, count);
	}

	printf("FINAL GSHARE CONTENTS\n");
	for (i=0 ; i < bp.gshare.params.num_entries ; i++) {
		base = i / bp.gshare.params.entries_per_byte;
		offset = i % bp.gshare.params.entries_per_byte;
		offset = offset * 2;
		byte = bp.gshare.pred_table.table[base];
		count = get_bits(byte, offset, offset + bp.gshare.params.bits_per_entry - 1);
		printf(" %d\t%d\n", i, count);
	}

	printf("FINAL BIMODAL CONTENTS\n");
	for (i=0 ; i < bp.bimodal.params.num_entries ; i++) {
		base = i / bp.bimodal.params.entries_per_byte;
		offset = i % bp.bimodal.params.entries_per_byte;
		offset = offset * 2;
		byte = bp.bimodal.pred_table.table[base];
		count = get_bits(byte, offset, offset + bp.bimodal.params.bits_per_entry - 1);
		printf(" %d\t%d\n", i, count);
	}

}

