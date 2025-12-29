#ifndef QUANTIZED_SPLAT_MODEL_HPP
#define QUANTIZED_SPLAT_MODEL_HPP
#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>
#ifndef TINYSPLAT_CODE_BOOK
#define TINYSPLAT_CODE_BOOK
namespace tinygsplat
{
	struct CodeBook
	{
		std::vector<uint8_t>    ids;
		std::vector<float>  centers;

		auto evaluate() const -> std::vector<float>
		{
			std::vector<float> r(ids.size());
#pragma omp parallel for
			for (auto i = 0; i < ids.size(); i++)
				r[i] = centers[ids[i]];
			return r;
		}
	};
}
#endif

class GaussianTrainModel;
class QuantizedStrategy
{
public:
    virtual void   quantized(GaussianTrainModel* model);
};

auto produceClusters(GaussianTrainModel* gaussian,int numClusters)->std::unordered_map<std::string, tinygsplat::CodeBook>;

#endif