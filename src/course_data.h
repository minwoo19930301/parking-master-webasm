#pragma once

#include <array>

namespace driving_test_data {

struct CenterInfo {
    const char* id;
    const char* name;
    const char* region;
    bool available;
};

inline constexpr std::array<CenterInfo, 27> kOfficialCenters = {{
    {"gangnam", "강남", "서울", false},
    {"dobong", "도봉", "서울", true},
    {"gangseo", "강서", "서울", false},
    {"seobu", "서부", "서울", false},
    {"busan-south", "부산남부", "부산", false},
    {"busan-north", "부산북부", "부산", false},
    {"daegu", "대구", "대구", false},
    {"incheon", "인천", "인천", false},
    {"yongin", "용인", "경기", false},
    {"ansan", "안산", "경기", false},
    {"uijeongbu", "의정부", "경기", false},
    {"chuncheon", "춘천", "강원", false},
    {"gangneung", "강릉", "강원", false},
    {"wonju", "원주", "강원", false},
    {"taebaek", "태백", "강원", false},
    {"cheongju", "청주", "충북", false},
    {"chungju", "충주", "충북", false},
    {"yesan", "예산", "충남", false},
    {"daejeon", "대전", "대전", false},
    {"jeonbuk", "전북", "전북", false},
    {"jeonnam", "전남", "전남", false},
    {"gwangyang", "광양", "전남", false},
    {"mungyeong", "문경", "경북", false},
    {"pohang", "포항", "경북", false},
    {"masan", "마산", "경남", false},
    {"ulsan", "울산", "울산", false},
    {"jeju", "제주", "제주", false},
}};

struct RoadSpec {
    float centerX;
    float centerY;
    float halfLength;
    float halfWidth;
    float angleRadians;
    bool centerLine;
    bool edgeLines;
};

struct RoutePoint {
    float x;
    float y;
};

namespace dobong_v1 {

inline constexpr const char* kPackId = "dobong-class-2-auto-2026-07";
inline constexpr const char* kDisplayName = "기능시험 규칙 연습 · 가상 배치";
inline constexpr const char* kRevision = "2026-07-30 reference build";
inline constexpr const char* kAccuracyNote =
    "기존 가상 배치의 규칙 연습용 코스. 실제 도봉 지형과 별개이며 측량 자료가 아님";

inline constexpr std::array<RoadSpec, 11> kRoads = {{
    {-18.0f, 30.0f, 32.0f, 4.0f, 0.0f, false, true},
    {12.0f, 16.0f, 18.0f, 4.0f, -1.57079632679f, false, true},
    {12.0f, 3.0f, 18.0f, 4.0f, 0.0f, false, true},
    {12.0f, -5.0f, 10.0f, 4.0f, -1.57079632679f, false, true},
    {-5.0f, -11.0f, 17.0f, 4.0f, 0.0f, false, true},
    {-12.0f, -19.0f, 9.0f, 4.0f, -1.57079632679f, false, true},
    {-25.0f, -20.0f, 10.0f, 4.0f, -1.57079632679f, false, true},
    {7.0f, -31.0f, 32.0f, 4.0f, 0.0f, false, true},
    {40.0f, -13.0f, 18.0f, 4.0f, 1.57079632679f, false, true},
    {40.0f, 10.0f, 20.0f, 4.0f, 1.57079632679f, false, true},
    {27.0f, 30.0f, 13.0f, 4.0f, 0.0f, false, true},
}};

inline constexpr std::array<RoutePoint, 16> kRoute = {{
    {-44.0f, 30.0f},
    {-25.0f, 30.0f},
    {-5.0f, 30.0f},
    {12.0f, 30.0f},
    {12.0f, 3.0f},
    {12.0f, -11.0f},
    {-12.0f, -11.0f},
    {-12.0f, -20.0f},
    {-12.0f, -11.0f},
    {-25.0f, -11.0f},
    {-25.0f, -31.0f},
    {20.0f, -31.0f},
    {40.0f, -31.0f},
    {40.0f, 10.0f},
    {40.0f, 30.0f},
    {27.0f, 30.0f},
}};

}  // namespace dobong_v1

}  // namespace driving_test_data
