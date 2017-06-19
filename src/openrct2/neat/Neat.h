#pragma once 

#include "Parameters.h"
#include "ActivationFunctions.h"

#include "../common.h"

typedef void* NeatGenome_t;
typedef void* NeatNetwork_t;
typedef void* NeatPopulation_t;

#ifdef __cplusplus
extern "C" {
#endif 

NeatGenome_t neat_create_genome(uint32_t id,
								uint32 numInputs,
								uint32 numHidden, 
								uint32 numOutputs, 
								bool FS, 
								NeatActivationFunction hiddenActFn,
								NeatActivationFunction outputActFn,
								uint32 seedType, 
								Neat_Parameters *params);
void neat_free_genome(NeatGenome_t genome);

NeatPopulation_t neat_create_population(NeatGenome_t root, Neat_Parameters* params, bool randomizeWeights, double randomRange, int rngSeed);
void neat_free_population(NeatPopulation_t pop);

size_t neat_population_species_size(NeatPopulation_t pop);
size_t neat_population_individuals_size(NeatPopulation_t pop, size_t species);

NeatGenome_t neat_get_individual(NeatPopulation_t pop, size_t species, size_t individual);
void neat_set_fitness(NeatPopulation_t pop, size_t species, size_t individual, double fitness);

NeatNetwork_t neat_create_network(NeatGenome_t individual);
void neat_destroy_network(NeatNetwork_t net);

void neat_next_epoch(NeatPopulation_t pop);

void neat_flush_net(NeatNetwork_t net);
void neat_activate_net(NeatNetwork_t net, double *inputs, size_t numInputs, double *outputs, size_t numOutputs);

#ifdef __cplusplus
}
#endif