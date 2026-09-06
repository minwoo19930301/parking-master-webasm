#pragma once
#include "road_routes.h"
namespace dobong_road_source {
// Pavement widths are practice estimates, NOT measured lane counts.
struct Road { const Point* points; std::size_t count; float width; bool oneWay; };
inline constexpr Point kRoad0[]={{291.767f,505.570f},{295.134f,522.591f},{332.598f,687.711f},{352.894f,757.909f},{360.156f,776.733f},{367.427f,793.420f},{416.375f,906.854f},{461.876f,1021.090f},{466.336f,1031.532f}};
inline constexpr Point kRoad1[]={{239.030f,240.229f},{246.486f,272.945f},{287.890f,481.558f},{291.767f,505.570f}};
inline constexpr Point kRoad2[]={{611.856f,-330.406f},{650.052f,-279.366f},{654.379f,-273.589f},{670.701f,-251.770f},{672.666f,-249.143f},{704.499f,-206.463f}};
inline constexpr Point kRoad3[]={{-22.215f,-969.425f},{-19.491f,-953.996f},{-17.024f,-939.146f},{43.372f,-654.379f},{52.828f,-608.415f},{58.204f,-581.543f}};
inline constexpr Point kRoad4[]={{-154.939f,-1617.872f},{-150.876f,-1597.868f},{-87.634f,-1286.607f},{-83.209f,-1265.323f}};
inline constexpr Point kRoad5[]={{215.006f,135.544f},{220.822f,158.086f},{226.031f,183.600f}};
inline constexpr Point kRoad6[]={{58.204f,-581.543f},{63.025f,-559.056f},{71.644f,-516.599f},{98.101f,-394.927f},{109.858f,-341.594f},{121.288f,-287.459f},{130.383f,-246.338f},{135.380f,-225.020f},{147.480f,-169.271f},{182.495f,-15.806f},{188.681f,12.547f},{194.718f,36.770f}};
inline constexpr Point kRoad7[]={{704.499f,-206.463f},{715.850f,-191.257f},{717.763f,-188.619f},{744.933f,-151.182f},{761.343f,-125.734f}};
inline constexpr Point kRoad8[]={{466.336f,1031.532f},{480.322f,1068.724f}};
inline constexpr Point kRoad9[]={{480.322f,1068.724f},{498.318f,1119.118f},{514.261f,1162.333f}};
inline constexpr Point kRoad10[]={{194.718f,36.770f},{209.639f,104.719f},{215.006f,135.544f}};
inline constexpr Point kRoad11[]={{226.031f,183.600f},{231.662f,206.666f},{239.030f,240.229f}};
inline constexpr Point kRoad12[]={{84.238f,-523.234f},{85.648f,-546.644f},{89.429f,-554.203f},{91.244f,-556.763f},{94.981f,-560.470f},{99.705f,-564.021f},{105.618f,-568.385f},{112.828f,-570.867f},{130.242f,-574.062f}};
inline constexpr Point kRoad13[]={{130.242f,-574.062f},{168.773f,-570.055f},{306.943f,-553.257f},{326.588f,-550.440f},{344.734f,-546.923f},{373.023f,-539.687f},{390.649f,-534.121f},{413.563f,-524.102f},{432.071f,-516.310f},{459.329f,-499.879f},{470.848f,-492.933f},{501.694f,-469.556f},{506.981f,-462.876f},{508.673f,-460.984f},{531.843f,-435.102f},{552.809f,-411.669f},{553.629f,-410.545f},{588.387f,-362.711f},{611.856f,-330.406f}};
inline constexpr Point kRoad14[]={{-47.358f,-590.215f},{3.555f,-586.118f},{37.485f,-583.391f},{58.204f,-581.543f},{72.869f,-580.029f},{90.090f,-578.248f},{130.242f,-574.062f}};
inline constexpr Point kRoad15[]={{25.587f,-188.084f},{121.782f,-217.038f},{123.192f,-217.473f},{135.380f,-225.020f},{148.256f,-228.126f}};
inline constexpr Point kRoad16[]={{492.405f,1060.820f},{476.744f,1022.482f}};
inline constexpr Point kRoad17[]={{531.173f,1156.433f},{512.798f,1113.241f},{494.590f,1066.442f},{492.405f,1060.820f}};
inline constexpr Point kRoad18[]={{476.744f,1022.482f},{471.853f,1009.714f},{428.475f,904.327f},{379.237f,790.448f},{371.913f,773.571f},{363.823f,754.703f},{344.707f,685.184f},{307.481f,519.796f},{304.123f,502.097f}};
inline constexpr Point kRoad19[]={{304.123f,502.097f},{300.325f,478.942f},{255.819f,269.561f},{249.562f,237.924f},{241.833f,203.772f},{236.933f,180.606f}};
inline constexpr Point kRoad20[]={{226.762f,132.204f},{221.386f,101.369f},{209.304f,33.386f}};
inline constexpr Point kRoad21[]={{236.933f,180.606f},{230.904f,156.038f},{226.762f,132.204f}};
inline constexpr Point kRoad22[]={{209.304f,33.386f},{200.834f,9.085f},{195.300f,-18.021f},{160.083f,-175.906f},{148.256f,-228.126f},{143.620f,-248.631f},{135.750f,-285.667f},{122.451f,-348.228f},{110.695f,-401.573f},{84.238f,-523.234f},{75.769f,-562.184f},{72.869f,-580.029f}};
inline constexpr Point kRoad23[]={{-83.209f,-1265.323f},{-79.614f,-1247.857f},{-22.215f,-969.425f}};
inline constexpr Point kRoad24[]={{72.869f,-580.029f},{66.506f,-610.275f},{56.274f,-655.760f},{-3.954f,-942.251f},{-5.902f,-956.923f},{-9.585f,-972.274f}};
inline constexpr Point kRoad25[]={{-9.585f,-972.274f},{-66.782f,-1250.395f},{-70.404f,-1268.006f}};
inline constexpr Point kRoad26[]={{-70.404f,-1268.006f},{-75.066f,-1288.900f},{-140.670f,-1600.628f},{-144.918f,-1619.152f}};
inline constexpr Road kRoads[]={
{kRoad0,sizeof(kRoad0)/sizeof(Point),9.f,true},
{kRoad1,sizeof(kRoad1)/sizeof(Point),9.f,true},
{kRoad2,sizeof(kRoad2)/sizeof(Point),14.f,false},
{kRoad3,sizeof(kRoad3)/sizeof(Point),9.f,true},
{kRoad4,sizeof(kRoad4)/sizeof(Point),9.f,true},
{kRoad5,sizeof(kRoad5)/sizeof(Point),9.f,true},
{kRoad6,sizeof(kRoad6)/sizeof(Point),9.f,true},
{kRoad7,sizeof(kRoad7)/sizeof(Point),14.f,false},
{kRoad8,sizeof(kRoad8)/sizeof(Point),9.f,true},
{kRoad9,sizeof(kRoad9)/sizeof(Point),9.f,true},
{kRoad10,sizeof(kRoad10)/sizeof(Point),9.f,true},
{kRoad11,sizeof(kRoad11)/sizeof(Point),9.f,true},
{kRoad12,sizeof(kRoad12)/sizeof(Point),9.f,true},
{kRoad13,sizeof(kRoad13)/sizeof(Point),14.f,false},
{kRoad14,sizeof(kRoad14)/sizeof(Point),14.f,false},
{kRoad15,sizeof(kRoad15)/sizeof(Point),6.f,false},
{kRoad16,sizeof(kRoad16)/sizeof(Point),9.f,true},
{kRoad17,sizeof(kRoad17)/sizeof(Point),9.f,true},
{kRoad18,sizeof(kRoad18)/sizeof(Point),9.f,true},
{kRoad19,sizeof(kRoad19)/sizeof(Point),9.f,true},
{kRoad20,sizeof(kRoad20)/sizeof(Point),9.f,true},
{kRoad21,sizeof(kRoad21)/sizeof(Point),9.f,true},
{kRoad22,sizeof(kRoad22)/sizeof(Point),9.f,true},
{kRoad23,sizeof(kRoad23)/sizeof(Point),9.f,true},
{kRoad24,sizeof(kRoad24)/sizeof(Point),9.f,true},
{kRoad25,sizeof(kRoad25)/sizeof(Point),9.f,true},
{kRoad26,sizeof(kRoad26)/sizeof(Point),9.f,true},
};
}
