#include <cstdlib>

void updateIds(
	const float *values,
	int *ids,
	const float *centers,
	const int n_values,
	const int n_centers);

void updateCenters(
    const float *values,
    const int *ids,
    float *centers,
    int *center_sizes,
    const int n_values,
    const int n_centers);
