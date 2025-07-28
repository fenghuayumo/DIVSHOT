#pragma once
#include <vector>
#include <functional>
#include <algorithm>
#include <cmath>

#include "editor_panel.h"
#include "editor.h"

#define INVALID_VALUE -1e10

namespace diverse
{
    class HistogramData
    {
    public:
        struct Bin
        {
            int selected;
            int unselected;
        };
        std::vector<Bin> bins;
        int numValues = 0;
        float minValue;
        float maxValue;

        HistogramData() {}
        HistogramData(int numBins)
        {
            for (auto i = 0; i < numBins; ++i) 
            {
                bins.push_back({ 0, 0 });
            }
        }

        void resize(int numBins)
        {
            for (auto i = 0; i < numBins; ++i) 
            {
                bins.push_back({ 0, 0 });
            }
        }

        void calc(int count, std::function<float(int)> valueFunc, std::function<bool(int)> selectedFunc)
        {
            if(count <=0 ) return;
            // clear bins
            for (int i = 0; i < bins.size(); ++i)
            {
                bins[i].selected = bins[i].unselected = 0;
            }

            // calculate min, max
            float min, max;
            int i;
            for (i = 0; i < count; i++)
            {
                float v = valueFunc(i);
                if (v != INVALID_VALUE)
                {
                    min = max = v;
                    break;
                }
            }

            // no data
            if (i == count)
            {
                return;
            }

            // continue min/max calc
            for (; i < count; i++)
            {
                float v = valueFunc(i);
                if (v != INVALID_VALUE)
                {
                    if (v < min)
                        min = v;
                    else if (v > max)
                        max = v;
                }
            }

            // fill bins
            for (int i = 0; i < count; i++)
            {
                float v = valueFunc(i);
                if (v != INVALID_VALUE)
                {
                    float n = min == max ? 0 : (v - min) / (max - min);
                    int bin = std::min<int>(bins.size() - 1, static_cast<int>(std::floor(n * bins.size())));
                    if (selectedFunc(i))
                        bins[bin].selected++;
                    else
                        bins[bin].unselected++;
                }
            }
            numValues = 0;
            for (auto& bin : bins)
            {
                numValues += bin.selected + bin.unselected;
            }
            minValue = min;
            maxValue = max;
        }

        float bucketValue(int bucket)
        {
            return minValue + bucket * bucketSize();
        }

        float bucketSize()
        {
            return (maxValue - minValue) / bins.size();
        }

        int valueToBucket(float value)
        {
            float n = minValue == maxValue ? 0 : (value - minValue) / (maxValue - minValue);
            return std::min(static_cast<int>(bins.size() - 1), static_cast<int>(std::floor(n * bins.size())));
        }

        std::pair<int,int> binMaxNumValues()
        {
            int maxNum = 0;
            int idx = 0;
            for (auto i = 0;i<bins.size();i++)
            {
                const auto& bin = bins[i];
                auto num = (bin.selected + bin.unselected);
                if (maxNum < num)
                {
                    maxNum = num;
                    idx = i;
                }
            }
            return {maxNum,idx};
        }
    };

    class HistogramPanel  : public EditorPanel
    {
    public:
       HistogramPanel(bool active = false);
        ~HistogramPanel() = default;

        void on_imgui_render() override;
        void on_new_scene(Scene* scene) override;

        void update_histogram(const ImVec2& sceneViewPosition,const ImVec2& sceneViewSize);
        void draw_panel_info(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize);
        HistogramData   histogram;

        bool            m_IsDraging = false;
        glm::vec2       m_DragStart;
        glm::vec2       m_DragEnd;
        int             m_SelectType = 0;
        int             m_LeftPanelWidth = 140;
    };
}