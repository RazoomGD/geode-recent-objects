#pragma once

#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>

#include <alphalaneous.editortab_api/include/EditorTabs.hpp>

#include <list>
#include <set>
#include <string>
#include <sstream>


// colors: 1-green, 2-blue, 3-pink, 4-gray, 5-darker gray, 6-red
// objects that should have darker button background
const std::set<short> darkerButtonBgObjIds = {
    146, 147, 204, 206, 673, 674, 1340, 1340, 1341, 1342, 1343, 1344, 1345,
    144, 145, 205, 459, 
    498, 499, 500, 501, 277, 278, 719, 721, 990, 992, 1120, 1122, 1123, 1124, 1125, 1126, 1127, 1132, 1133,
    1134, 1135, 1136, 1137, 1138, 1139, 1241, 1242, 1243, 1244, 1245, 1246,
    1292, 1010, 1009, 1271, 1272, 1760, 1761, 1887, 1011, 1012, 1013, 1269, 1270, 1293, 1762, 1763, 1888,
    740, 741, 742
};
