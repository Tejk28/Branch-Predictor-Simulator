#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>

#include "sim_bp.h"

void initialize_pred_params(predictor_t *predictor)
{
	if (strcmp(sim_type, "bimodal") == 0) {
		strcpy(predictor->params.sim_type, sim_type);
		predictor->params.M2 = M2;
		predictor->params.M1 = 0;
		predictor->params.N = 0;
		predictor->params.K = 0;
	} else if (strcmp(sim_type, "gshare") == 0) {
		strcpy(predictor->params.sim_type, sim_type);
		predictor->params.M2 = 0;
		predictor->params.M1 = M1;
		predictor->params.N = N;
		predictor->params.K = 0;
	} 
}

int get_bits(int byte, int start, int end)
{
       int result = ((byte >> start) & ~(~0 << (end-start+1)));
       return result;
}

int update_bits(int byte, int start, int end, int new_val)
{
        int prev_val = get_bits(byte, start, end);
        prev_val = prev_val << start;
        byte = byte ^ prev_val;
        new_val = new_val << start;
        byte = byte ^ new_val;
        return byte;
}
void allocate_and_init_pred_tab(predictor_t *predictor)
{
	int i;

	if (strcmp(predictor->params.sim_type, "bimodal") == 0) {

		predictor->params.num_entries = pow(2, predictor->params.M2);
		predictor->params.bits_per_entry = COUNTER_BITS;
		predictor->pred_table.size_in_bytes = predictor->params.num_entries * predictor->params.bits_per_entry / BITS_PER_BYTE;
		predictor->params.entries_per_byte = BITS_PER_BYTE / predictor->params.bits_per_entry;

		predictor->pred_table.table = (unsigned char *)malloc(sizeof(unsigned char) * predictor->pred_table.size_in_bytes);
		if (predictor->pred_table.table == NULL) {
			printf("Error while allocating Memory!!\n");
			printf("Exiting...\n");
			exit(1);
		}

		for (i=0 ; i < predictor->pred_table.size_in_bytes ; i++) {
			predictor->pred_table.table[i] = 0xAA;
		}
	} else if (strcmp(predictor->params.sim_type, "gshare") == 0) {

		predictor->params.num_entries = pow(2, predictor->params.M1);
		predictor->params.bits_per_entry = COUNTER_BITS;
		predictor->pred_table.size_in_bytes = predictor->params.num_entries * predictor->params.bits_per_entry / BITS_PER_BYTE;
		predictor->params.entries_per_byte = BITS_PER_BYTE / predictor->params.bits_per_entry;

		predictor->pred_table.table = (unsigned char *)malloc(sizeof(unsigned char) * predictor->pred_table.size_in_bytes);
		if (predictor->pred_table.table == NULL) {
			printf("Error while allocating Memory!!\n");
			printf("Exiting...\n");
			exit(1);
		}

		/* Initialize all the counters in Prediction Table to 2 (weakly taken) */
		for (i=0 ; i < predictor->pred_table.size_in_bytes ; i++) {
			predictor->pred_table.table[i] = 0xAA;
		}

		/* Initialize Branch History Register to all Zeros*/
		predictor->bhr = 0;
	}
}



void handle_branch_prediction(predictor_t *predictor, pc_t *pc)
{
	int index;
	int base, offset, byte, count;
	int m_pc_bits, n_pc_bits, lower_pc_bits, msb_bhr;
	int this_prediction=0 ; 
  // is_misprediction=0;

	if (strcmp(predictor->params.sim_type, "bimodal") == 0) {

#ifdef DEBUG_OP
		printf("=%ld\t%x %c\n", predictor->params.num_predictions, pc->addr, pc->branch_outcome);
#endif

		index = get_bits(pc->addr, 2, predictor->params.M2 + 1);
                base = index / predictor->params.entries_per_byte;
                offset = index % predictor->params.entries_per_byte;
                offset = offset * 2;
                byte = predictor->pred_table.table[base];
                count = get_bits(byte, offset, offset + predictor->params.bits_per_entry - 1);
#ifdef DEBUG_OP
		printf("\tBP: %d %d\n", index, count);
#endif

		if (count == 0 || count == 1) {
			this_prediction = NOT_TAKEN;
		} else if (count == 2 || count == 3) {
			this_prediction = TAKEN;
		}

	//	is_misprediction = 0;
		if (pc->branch_outcome == 'n' && this_prediction == TAKEN) {
		//	is_misprediction = 1;
			predictor->params.num_mispredictions += 1;
		} else if (pc->branch_outcome == 't' && this_prediction == NOT_TAKEN) {
		//	is_misprediction = 1;
			predictor->params.num_mispredictions += 1;
		}

		if (pc->branch_outcome == 't') {
			if (count < 3)
				count += 1;
		} else if (pc->branch_outcome == 'n') {
			if (count > 0)
				count -= 1;
		}

		predictor->pred_table.table[base] = update_bits(byte, offset, offset + predictor->params.bits_per_entry - 1, count);

#ifdef DEBUG_OP
		printf("\tBU: %d %d\n", index, count);
#endif

	} else if (strcmp(predictor->params.sim_type, "gshare") == 0) {

#ifdef DEBUG_OP
		printf("=%ld\t%x %c\n", predictor->params.num_predictions, pc->addr, pc->branch_outcome);
#endif

		/* Extracting index:
			1) First we extract 2 to M1+1 bits of pc - number of bits extracted will be M1.
			2) Then from these M1 bits, we extract upper N bits.
			3) XOR these upper N bits with N-bits long BHR (Branch History Register).
			4) Append the lower bits (n-m) of pc extracted bits to result of XOR - forming the index.
		*/
		m_pc_bits = get_bits(pc->addr, 2, predictor->params.M1 + 1);
		lower_pc_bits = get_bits(m_pc_bits, 0, predictor->params.M1 - predictor->params.N - 1);
		n_pc_bits = get_bits(m_pc_bits, predictor->params.M1 - predictor->params.N, predictor->params.M1 - 1);

		index = n_pc_bits ^ predictor->bhr;
		index = index << (predictor->params.M1 - predictor->params.N);
		index = index ^ lower_pc_bits;

		/* The index will provide a unique prediction counter.
		   However, since each counter is a 2 bit entity, index is first split into base and offset.
		   Base points to the byte which contains the required indexed count. Offset points to the first bit of the two-bit
		   counter within the byte indexed by the base.
		*/
		base = index / predictor->params.entries_per_byte;
		offset = index % predictor->params.entries_per_byte;
		offset = offset * 2;
		byte = predictor->pred_table.table[base];
		count = get_bits(byte, offset, offset + predictor->params.bits_per_entry - 1);

#ifdef DEBUG_OP
		printf("\tGP: %d %d\n", index, count);
#endif

		if (count == 0 || count == 1) {
			this_prediction = NOT_TAKEN;
		} else if (count == 2 || count == 3) {
			this_prediction = TAKEN;
		}

	//	is_misprediction = 0;
		if (pc->branch_outcome == 'n' && this_prediction == TAKEN) {
	//		is_misprediction = 1;
			predictor->params.num_mispredictions += 1;
		} else if (pc->branch_outcome == 't' && this_prediction == NOT_TAKEN) {
	//		is_misprediction = 1;
			predictor->params.num_mispredictions += 1;
		}

		if (pc->branch_outcome == 't') {
			if (count < 3)
				count += 1;
		} else if (pc->branch_outcome == 'n') {
			if (count > 0)
				count -= 1;
		}

		predictor->pred_table.table[base] = update_bits(byte, offset, offset + predictor->params.bits_per_entry - 1, count);

		msb_bhr = 0;
		if (pc->branch_outcome == 'n')
			msb_bhr = 0;
		else if (pc->branch_outcome == 't')
			msb_bhr = 1;

		/* Updating BHR (Branch History Register)
		   BHR is shifted Right by one bit.
		   The actual outcome of the branch is then put as the Most Significant Bit of the BHR (i.e. N-1th bit of BHR).
		   This updated BHR will then be used in the Prediction stage while XORing with PC bits. (See Above).
		*/
		predictor->bhr = predictor->bhr >> 1;
		msb_bhr = msb_bhr << (predictor->params.N - 1);
		predictor->bhr = predictor->bhr ^ msb_bhr;

#ifdef DEBUG_OP
		printf("\tGU: %d %d\n", index, count);
#endif

	}
}

void init_hybrid_pred_params(branch_predictor *bp)
{
	/* Initialize Hybrid Branch Predictor Parameters. */
	strcpy(bp->params.sim_type, sim_type);
	bp->params.K = K;
	bp->params.M1 = M1;
	bp->params.N = N;
	bp->params.M2 = M2;

	/* Initialize Bimodal Branch Predictor Parameters. */
	strcpy(bp->bimodal.params.sim_type, "bimodal");
	bp->bimodal.params.M2 = M2;
	bp->bimodal.params.M1 = 0;
	bp->bimodal.params.N = 0;
	bp->bimodal.params.K = 0;

	/* initialize GShare Branch Predictor Parameters. */
	strcpy(bp->gshare.params.sim_type, "gshare");
	bp->gshare.params.M2 = 0;
	bp->gshare.params.M1 = M1;
	bp->gshare.params.N = N;
	bp->gshare.params.K = 0;
}

void allocate_and_init_hybrid_pred_tab(branch_predictor *bp)
{
	int i;

	/* Allocate and Assign Hybrid Branch Predictor Parameters. */
	bp->params.num_entries = pow(2, bp->params.K);
	bp->params.bits_per_entry = COUNTER_BITS;
	bp->chooser_table.size_in_bytes = bp->params.num_entries * bp->params.bits_per_entry / BITS_PER_BYTE;
	bp->params.entries_per_byte = BITS_PER_BYTE / bp->params.bits_per_entry;

	bp->chooser_table.table = (unsigned char *)malloc(sizeof(unsigned char) * bp->chooser_table.size_in_bytes);
	if (bp->chooser_table.table == NULL) {
		printf("Error while allocating Memory!!\n");
		printf("Exiting...\n");
		exit(1);
	}

	for (i=0 ; i < bp->chooser_table.size_in_bytes ; i++) {
		bp->chooser_table.table[i] = 0x55;
	}

	/* Allocate and Assign Bimodal Branch Predictor Parameters. */
	bp->bimodal.params.num_entries = pow(2, bp->bimodal.params.M2);
	bp->bimodal.params.bits_per_entry = COUNTER_BITS;
	bp->bimodal.pred_table.size_in_bytes = bp->bimodal.params.num_entries * bp->bimodal.params.bits_per_entry / BITS_PER_BYTE;
	bp->bimodal.params.entries_per_byte = BITS_PER_BYTE / bp->bimodal.params.bits_per_entry;

	bp->bimodal.pred_table.table = (unsigned char *)malloc(sizeof(unsigned char) * bp->bimodal.pred_table.size_in_bytes);
	if (bp->bimodal.pred_table.table == NULL) {
		printf("Error while allocating Memory!!\n");
		printf("Exiting...\n");
		exit(1);
	}

	for (i=0 ; i < bp->bimodal.pred_table.size_in_bytes ; i++) {
		bp->bimodal.pred_table.table[i] = 0xAA;
	}

	/* Allocate and Assign GShare Branch Predictor Parameters. */
	bp->gshare.params.num_entries = pow(2, bp->gshare.params.M1);
	bp->gshare.params.bits_per_entry = COUNTER_BITS;
	bp->gshare.pred_table.size_in_bytes = bp->gshare.params.num_entries * bp->gshare.params.bits_per_entry / BITS_PER_BYTE;
	bp->gshare.params.entries_per_byte = BITS_PER_BYTE / bp->gshare.params.bits_per_entry;

	bp->gshare.pred_table.table = (unsigned char *)malloc(sizeof(unsigned char) * bp->gshare.pred_table.size_in_bytes);
	if (bp->gshare.pred_table.table == NULL) {
		printf("Error while allocating Memory!!\n");
		printf("Exiting...\n");
		exit(1);
	}

	for (i=0 ; i < bp->gshare.pred_table.size_in_bytes ; i++) {
		bp->gshare.pred_table.table[i] = 0xAA;
	}

	bp->gshare.bhr = 0;

}

int get_bimodal_prediction(predictor_t *predictor, pc_t *pc)
{
	int index, base, offset, byte, count;
	int this_prediction=0;

	index = get_bits(pc->addr, 2, predictor->params.M2 + 1);
	base = index / predictor->params.entries_per_byte;
	offset = index % predictor->params.entries_per_byte;
	offset = offset * 2;
	byte = predictor->pred_table.table[base];
	count = get_bits(byte, offset, offset + predictor->params.bits_per_entry - 1);

#ifdef DEBUG_OP
	printf("\tBP: %d %d\n", index, count);
#endif

	if (count == 0 || count == 1) {
		this_prediction = NOT_TAKEN;
	} else if (count == 2 || count == 3) {
		this_prediction = TAKEN;
	}

	return (this_prediction);
}

void update_bimodal_predictor(predictor_t *predictor, int prediction, pc_t *pc)
{
	//int is_misprediction = 0;
	int base, offset, byte, count, index;

	index = get_bits(pc->addr, 2, predictor->params.M2 + 1);

	base = index / predictor->params.entries_per_byte;
	offset = index % predictor->params.entries_per_byte;
	offset = offset * 2;
	byte = predictor->pred_table.table[base];
	count = get_bits(byte, offset, offset + predictor->params.bits_per_entry - 1);

	if (pc->branch_outcome == 'n' && prediction == TAKEN) {
		//is_misprediction = 1;
		predictor->params.num_mispredictions += 1;
	} else if (pc->branch_outcome == 't' && prediction == NOT_TAKEN) {
		//is_misprediction = 1;
		predictor->params.num_mispredictions += 1;
	}

	if (pc->branch_outcome == 't') {
		if (count < 3)
			count += 1;
	} else if (pc->branch_outcome == 'n') {
		if (count > 0)
			count -= 1;
	}

	predictor->pred_table.table[base] = update_bits(byte, offset, offset + predictor->params.bits_per_entry - 1, count);

#ifdef DEBUG_OP
	printf("\tBU: %d %d\n", index, count);
#endif
}

int get_gshare_prediction(predictor_t *predictor, pc_t *pc)
{
	int index, base, offset, byte, count;
	int m_pc_bits, lower_pc_bits, n_pc_bits;
	int this_prediction=0;

	/* Extracting index:
		1) First we extract 2 to M1+1 bits of pc - number of bits extracted will be M1.
		2) Then from these M1 bits, we extract upper N bits.
		3) XOR these upper N bits with N-bits long BHR (Branch History Register).
		4) Append the lower bits (n-m) of pc extracted bits to result of XOR - forming the index.
	*/
	m_pc_bits = get_bits(pc->addr, 2, predictor->params.M1 + 1);
	lower_pc_bits = get_bits(m_pc_bits, 0, predictor->params.M1 - predictor->params.N - 1);
	n_pc_bits = get_bits(m_pc_bits, predictor->params.M1 - predictor->params.N, predictor->params.M1 - 1);

	index = n_pc_bits ^ predictor->bhr;
	index = index << (predictor->params.M1 - predictor->params.N);
	index = index ^ lower_pc_bits;

	/* The index will provide a unique prediction counter.
	   However, since each counter is a 2 bit entity, index is first split into base and offset.
	   Base points to the byte which contains the required indexed count. Offset points to the first bit of the two-bit
	   counter within the byte indexed by the base.
	*/
	base = index / predictor->params.entries_per_byte;
	offset = index % predictor->params.entries_per_byte;
	offset = offset * 2;
	byte = predictor->pred_table.table[base];
	count = get_bits(byte, offset, offset + predictor->params.bits_per_entry - 1);

#ifdef DEBUG_OP
	printf("\tGP: %d %d\n", index, count);
#endif

	if (count == 0 || count == 1) {
		this_prediction = NOT_TAKEN;
	} else if (count == 2 || count == 3) {
		this_prediction = TAKEN;
	}

	return (this_prediction);
}

void update_gshare_predictor(predictor_t *predictor, int prediction, pc_t *pc)
{
	int base, offset, byte, count, index;
	int m_pc_bits, lower_pc_bits, n_pc_bits;
//	int is_misprediction;

	m_pc_bits = get_bits(pc->addr, 2, predictor->params.M1 + 1);
	lower_pc_bits = get_bits(m_pc_bits, 0, predictor->params.M1 - predictor->params.N - 1);
	n_pc_bits = get_bits(m_pc_bits, predictor->params.M1 - predictor->params.N, predictor->params.M1 - 1);

	index = n_pc_bits ^ predictor->bhr;
	index = index << (predictor->params.M1 - predictor->params.N);
	index = index ^ lower_pc_bits;

	base = index / predictor->params.entries_per_byte;
	offset = index % predictor->params.entries_per_byte;
	offset = offset * 2;
	byte = predictor->pred_table.table[base];
	count = get_bits(byte, offset, offset + predictor->params.bits_per_entry - 1);

//	is_misprediction = 0;
	if (pc->branch_outcome == 'n' && prediction == TAKEN) {
	//	is_misprediction = 1;
		predictor->params.num_mispredictions += 1;
	} else if (pc->branch_outcome == 't' && prediction == NOT_TAKEN) {
	//	is_misprediction = 1;
		predictor->params.num_mispredictions += 1;
	}

	if (pc->branch_outcome == 't') {
		if (count < 3)
			count += 1;
	} else if (pc->branch_outcome == 'n') {
		if (count > 0)
			count -= 1;
	}

	predictor->pred_table.table[base] = update_bits(byte, offset, offset + predictor->params.bits_per_entry - 1, count);

#ifdef DEBUG_OP
	printf("\tBU: %d %d\n", index, count);
#endif
}

void update_gshare_bhr(predictor_t *predictor, pc_t *pc)
{
	int msb_bhr;

	msb_bhr = 0;
	if (pc->branch_outcome == 'n')
		msb_bhr = 0;
	else if (pc->branch_outcome == 't')
		msb_bhr = 1;

	/* Updating BHR (Branch History Register)
	*/
	predictor->bhr = predictor->bhr >> 1;
	msb_bhr = msb_bhr << (predictor->params.N - 1);
	predictor->bhr = predictor->bhr ^ msb_bhr;
}

void handle_hybrid_branch_prediction(branch_predictor *bp, pc_t *pc)
{

	int bimodal_prediction, gshare_prediction;
	int index, offset, base, byte, chooser_count;
	int this_prediction=0, actual_outcome=0;

#ifdef DEBUG_OP
	printf("=%ld\t%x %c\n", bp->params.num_predictions, pc->addr, pc->branch_outcome);
#endif

	gshare_prediction = get_gshare_prediction(&bp->gshare, pc);
	bimodal_prediction = get_bimodal_prediction(&bp->bimodal, pc);

	index = get_bits(pc->addr, 2, bp->params.K + 1);
	base = index / bp->params.entries_per_byte;
	offset = index % bp->params.entries_per_byte;
	offset = offset * 2;
	byte = bp->chooser_table.table[base];
	chooser_count = get_bits(byte, offset, offset + bp->params.bits_per_entry - 1);

#ifdef DEBUG_OP
	printf("\tCP: %d %d\n", index, chooser_count);
#endif

	if (chooser_count == 0 || chooser_count == 1) {
		this_prediction = bimodal_prediction;
		update_bimodal_predictor(&bp->bimodal, bimodal_prediction, pc);
	} else if (chooser_count == 2 || chooser_count == 3) {
		this_prediction = gshare_prediction;
		update_gshare_predictor(&bp->gshare, gshare_prediction, pc);
	}

	update_gshare_bhr(&bp->gshare, pc);

	if (pc->branch_outcome == 'n')
		actual_outcome = NOT_TAKEN;
	else if (pc->branch_outcome == 't')
		actual_outcome = TAKEN;

	if (gshare_prediction == actual_outcome && bimodal_prediction != actual_outcome) {
		if (chooser_count < 3)
			chooser_count += 1;
		bp->chooser_table.table[base] = update_bits(byte, offset, offset + bp->params.bits_per_entry - 1, chooser_count);
#ifdef DEBUG_OP
		printf("\tCU: %d %d\n", index, chooser_count);
#endif
	} else if (gshare_prediction != actual_outcome && bimodal_prediction == actual_outcome) {
		if (chooser_count > 0)
			chooser_count -= 1;
		bp->chooser_table.table[base] = update_bits(byte, offset, offset + bp->params.bits_per_entry - 1, chooser_count);
#ifdef DEBUG_OP
		printf("\tCU: %d %d\n", index, chooser_count);
#endif
	}

	if (this_prediction != actual_outcome)
		bp->params.num_mispredictions += 1;

}

