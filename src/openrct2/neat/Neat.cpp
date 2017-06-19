#include "Neat.h"

#include "Genome.h"
#include "Population.h"
#include "NeuralNetwork.h"
#include "Substrate.h"

#include <iostream>
#include <ctime>

using namespace NEAT;

#ifdef __cplusplus
extern "C" {
#endif

NeatGenome_t neat_create_genome(uint32_t id, uint32 numInputs, uint32 numHidden, uint32 numOutputs, bool FS, NeatActivationFunction hiddenActFn, NeatActivationFunction outputActFn, uint32 seedType, Neat_Parameters *params)
{
	NEAT::Genome *genome = new NEAT::Genome(id, 
											numInputs, 
											numHidden, 
											numOutputs, 
											FS, 
											outputActFn, 
											hiddenActFn, 
											seedType, 
											*static_cast<NEAT::NeatParameters*>(params));

	return (NeatGenome_t)genome;
}

void neat_free_genome(NeatGenome_t genome)
{
	if (genome)
		delete (NEAT::Genome*)genome;
}

NeatPopulation_t neat_create_population(NeatGenome_t root, Neat_Parameters* params, bool randomizeWeights, double randomRange, int rngSeed)
{
	NEAT::Population *pop = new NEAT::Population(*(NEAT::Genome*)root, 
												 *static_cast<NEAT::NeatParameters*>(params), 
												 randomizeWeights, 
												 randomRange, 
												 rngSeed);
	return (NeatPopulation_t*)pop;

}

void neat_free_population(NeatPopulation_t pop)
{
	if (pop)
		delete (NEAT::Population*)pop;
}

size_t neat_population_species_size(NeatPopulation_t pop)
{
	if (!pop)
		return 0;
	
	return ((NEAT::Population*)pop)->m_Species.size();
}

size_t neat_population_individuals_size(NeatPopulation_t pop, size_t species)
{
	if (!pop)
		return 0;

	return ((NEAT::Population*)pop)->m_Species[species].m_Individuals.size();
}

NeatGenome_t neat_get_individual(NeatPopulation_t pop, size_t species, size_t individual)
{
	if (!pop)
		return 0;

	return (NeatGenome_t)&((NEAT::Population*)pop)->m_Species[species].m_Individuals[individual];
}

void neat_set_fitness(NeatPopulation_t pop, size_t species, size_t individual, double fitness)
{
	((NEAT::Population*)pop)->m_Species[species].m_Individuals[individual].SetFitness(fitness);
	((NEAT::Population*)pop)->m_Species[species].m_Individuals[individual].SetEvaluated();
}

NeatNetwork_t neat_create_network(NeatGenome_t individual)
{
	NEAT::NeuralNetwork *net = new NEAT::NeuralNetwork();
	
	((NEAT::Genome*)individual)->BuildPhenotype(*net);

	return net;
}

void neat_destroy_network(NeatNetwork_t net)
{
	if (!net)
		return;

	delete (NEAT::NeuralNetwork *)net;
}

void neat_next_epoch(NeatPopulation_t pop)
{
	if (!pop)
		return;

	((NEAT::Population*)pop)->Epoch();
}

void neat_flush_net(NeatNetwork_t net)
{
	((NEAT::NeuralNetwork *)net)->Flush();
}

void neat_activate_net(NeatNetwork_t net, double *inputs, size_t numInputs, double *outputs, size_t numOutputs)
{
	if (!net)
		return;

	static std::vector<double> inp;
	inp.resize(numInputs);
	for (size_t n = 0; n < numInputs; n++)
		inp[n] = inputs[n];

	((NEAT::NeuralNetwork *)net)->Input(inp);
	((NEAT::NeuralNetwork *)net)->Activate();

	static std::vector<double> out;
	out = std::move(((NEAT::NeuralNetwork *)net)->Output());

	for (size_t n = 0; n < numOutputs; n++)
	{
		outputs[n] = out[n];
	}
}

#ifdef __cplusplus
}
#endif
