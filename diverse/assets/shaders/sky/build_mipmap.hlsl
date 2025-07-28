

//Present code requires 16x16 group dimensions
#define PDF_SAMPLING_GROUP_SIZE 16


[[vk::binding(0)]]  RWTexture2D<float> LightPdfUAV0;
[[vk::binding(1)]]  RWTexture2D<float> LightPdfUAV1;
[[vk::binding(2)]]  RWTexture2D<float> LightPdfUAV2;
[[vk::binding(3)]]  RWTexture2D<float> LightPdfUAV3;
[[vk::binding(4)]]  RWTexture2D<float> LightPdfUAV4;

[[vk::binding(5)]]  Texture2D<float> LightPdfTexture;


[[vk::binding(6)]] cbuffer _ {
   int  BaseLevel;
   uint PdfTexDimensions;
   uint2 padding; // Padding to align to 16 bytes
};


groupshared float Cache[PDF_SAMPLING_GROUP_SIZE][PDF_SAMPLING_GROUP_SIZE];

[numthreads(PDF_SAMPLING_GROUP_SIZE, PDF_SAMPLING_GROUP_SIZE, 1)]
void main(uint3 GroupIndex : SV_GroupID, uint3 LocalIndex : SV_GroupThreadID)
{
	uint2 Cell = LocalIndex.xy + GroupIndex.xy * PDF_SAMPLING_GROUP_SIZE;

	float Pdf = 0.0;

	uint ActiveWidth = min(PdfTexDimensions, PDF_SAMPLING_GROUP_SIZE);

	if (all(LocalIndex.xy < ActiveWidth.xx))
	{
		if (BaseLevel == 0)
		{
			// empty  beacuse we have calculate pdf in another pass
		}
		else
		{
			// fetching from the previous level and downsampling
			uint2 SampleBase = Cell * 2;
			
			Pdf = LightPdfTexture.Load(int3(SampleBase + uint2(0, 0), 0));
			Pdf += LightPdfTexture.Load(int3(SampleBase + uint2(0, 1), 0));
			Pdf += LightPdfTexture.Load(int3(SampleBase + uint2(1, 0), 0));
			Pdf += LightPdfTexture.Load(int3(SampleBase + uint2(1, 1), 0));

			Pdf *= 0.25;
		}

		LightPdfUAV0[Cell] = Pdf;
	}

	// always write to the entire cache
	Cache[LocalIndex.y][LocalIndex.x] = Pdf;

	//   * GroupSync barriers inside uniform control flow aren't supported by all shader compilers
	ActiveWidth = ActiveWidth / 2;

	GroupMemoryBarrierWithGroupSync();

	// 8x8
	if (all(LocalIndex.xy < ActiveWidth.xx))
	{
		uint2 SourceCell = LocalIndex.xy * 2;
		Pdf = Cache[SourceCell.y][SourceCell.x] * 0.25;
		Pdf += Cache[SourceCell.y][SourceCell.x + 1] * 0.25;
		Pdf += Cache[SourceCell.y + 1][SourceCell.x] * 0.25;
		Pdf += Cache[SourceCell.y + 1][SourceCell.x + 1] * 0.25;
		LightPdfUAV1[LocalIndex.xy + GroupIndex.xy*ActiveWidth] = Pdf;
	}

	GroupMemoryBarrierWithGroupSync();
	if (all(LocalIndex.xy < ActiveWidth.xx))
	{
		Cache[LocalIndex.y][LocalIndex.x] = Pdf;
	}
	ActiveWidth = ActiveWidth / 2;
	GroupMemoryBarrierWithGroupSync();

	// 4x4
	if (all(LocalIndex.xy < ActiveWidth.xx))
	{
		uint2 SourceCell = LocalIndex.xy * 2;
		Pdf = Cache[SourceCell.y][SourceCell.x] * 0.25;
		Pdf += Cache[SourceCell.y][SourceCell.x + 1] * 0.25;
		Pdf += Cache[SourceCell.y + 1][SourceCell.x] * 0.25;
		Pdf += Cache[SourceCell.y + 1][SourceCell.x + 1] * 0.25;
		LightPdfUAV2[LocalIndex.xy + GroupIndex.xy * ActiveWidth] = Pdf;
	}

	GroupMemoryBarrierWithGroupSync();
	if (all(LocalIndex.xy < ActiveWidth))
	{
		Cache[LocalIndex.y][LocalIndex.x] = Pdf;
	}
	ActiveWidth = ActiveWidth / 2;
	GroupMemoryBarrierWithGroupSync();

	// 2x2
	if (all(LocalIndex.xy < ActiveWidth.xx))
	{
		uint2 SourceCell = LocalIndex.xy * 2;
		Pdf = Cache[SourceCell.y][SourceCell.x] * 0.25;
		Pdf += Cache[SourceCell.y][SourceCell.x + 1] * 0.25;
		Pdf += Cache[SourceCell.y + 1][SourceCell.x] * 0.25;
		Pdf += Cache[SourceCell.y + 1][SourceCell.x + 1] * 0.25;
		LightPdfUAV3[LocalIndex.xy + GroupIndex.xy * ActiveWidth] = Pdf;
	}

	GroupMemoryBarrierWithGroupSync();
	if (all(LocalIndex.xy < ActiveWidth.xx))
	{
		Cache[LocalIndex.y][LocalIndex.x] = Pdf;
	}
	ActiveWidth = ActiveWidth / 2;
	GroupMemoryBarrierWithGroupSync();

	// 1x1
	if (all(LocalIndex.xy < ActiveWidth.xx))
	{
		uint2 SourceCell = LocalIndex.xy * 2;
		Pdf = Cache[SourceCell.y][SourceCell.x] * 0.25;
		Pdf += Cache[SourceCell.y][SourceCell.x + 1] * 0.25;
		Pdf += Cache[SourceCell.y + 1][SourceCell.x] * 0.25;
		Pdf += Cache[SourceCell.y + 1][SourceCell.x + 1] * 0.25;
		LightPdfUAV4[LocalIndex.xy + GroupIndex.xy * ActiveWidth] = Pdf;
	}
}

