| 名称 | 类型 | 声明 | 显式写入/初始化 | 其它引用 |
|---|---|---|---|---|
| `diagnostic_build` | `const char*` | `main.cpp:38` | `main.cpp:38,40` | `main.cpp:49,517,642` |
| `diagnostic_exe` | `wchar_t[MAX_PATH]` | `main.cpp:43` | `main.cpp:43` | `main.cpp:44,45,63` |
| `diagnostic_path` | `std::wstring` | `main.cpp:45` | `main.cpp:45,47,48` | `main.cpp:45,46,47,662,1085` |
| `diagnostic_slash` | `const auto` | `main.cpp:46` | `main.cpp:46` | `main.cpp:47` |
| `serial_axis1_handle` | `constexpr DWORD` | `main.cpp:51` | `main.cpp:51` | `main.cpp:75,447,578,602` |
| `serial_axis6_handle` | `constexpr DWORD` | `main.cpp:52` | `main.cpp:52` | `main.cpp:76,448,586,615` |
| `ads_snapshot_wait_timeout_ms` | `constexpr DWORD` | `main.cpp:54` | `main.cpp:54` | `main.cpp:460` |
| `vis_publish_rate_hz` | `constexpr std::int64_t` | `main.cpp:56` | `main.cpp:56` | `main.cpp:2298` |
| `axis4_coupled_axis6_speed_mm_s` | `constexpr double` | `main.cpp:58` | `main.cpp:58` | `main.cpp:1718` |
| `hardcoded_ads_netid` | `const char*` | `main.cpp:59` | `main.cpp:59` | — |
| `cfg` | `ControlConfig` | `main.cpp:61` | `main.cpp:61` | `main.cpp:61,109,110,127,128,156,233,321,903,904,908,909,936,937,939,982,997,1310,1312,1327,1331,1333,1354,1376,1446,1447,1448,1449,1454,1455,1467,1479,1481,1483,1484,1507,1529,1530,1531,1538,1561,1571,1579,1604,1619,1646,1650,1652,1655,1675,1676,1678,1734,1738,1739,1740,1742,1778,1794,1847,1858,1859,1861,1900,1901,1902,1904,1906,1958,1973,2210,2503,2506` |
| `cyl` | `const CylinderPreset` | `main.cpp:62` | `main.cpp:62` | `main.cpp:62,157,1253,1254,1255,1256,1450,1451,1568,1569,1570,1799,1800,1801,1802,2015,2016,2028,2029,2035,2036,2040,2041,2179,2183` |
| `run_directory` | `const std::filesystem::path` | `main.cpp:63` | `main.cpp:63` | `main.cpp:64,65,87` |
| `homing_record_path` | `const auto` | `main.cpp:64` | `main.cpp:64` | `main.cpp:66,1185,1193` |
| `homing_reuse_enabled` | `const bool` | `main.cpp:65` | `main.cpp:65` | `main.cpp:843,848` |
| `homing_record` | `homing::Record` | `main.cpp:66` | `main.cpp:66,1174,1175,1176,1177,1180,1181,1182,1191,1192` | `main.cpp:843,849,850,860,1177,1185,1187,1193` |
| `plc_application_name` | `std::string` | `main.cpp:67` | `main.cpp:67,785` | `main.cpp:67,843,850,1175` |
| `homing_reuse_pending` | `bool` | `main.cpp:68` | `main.cpp:68,794,852,867,883` | `main.cpp:778,811,1187` |
| `homing_restore_submitted` | `bool` | `main.cpp:68` | `main.cpp:68,795,862,882` | `main.cpp:811,864,865,874,1122,1187,1195` |
| `control_before_plc_restart` | `bool` | `main.cpp:69` | `main.cpp:69,793` | `main.cpp:890,891,1153` |
| `homing_restore_applied` | `bool` | `main.cpp:69` | `main.cpp:69,888,1152` | `main.cpp:1136,1151` |
| `homing_record_update_ms` | `ULONGLONG` | `main.cpp:70` | `main.cpp:70,1184,1190` | `main.cpp:1188` |
| `homing_record_pending` | `bool` | `main.cpp:71` | `main.cpp:71,796,1138,1173` | `main.cpp:1170,1187` |
| `homing_record_refresh_generation` | `std::uint64_t` | `main.cpp:72` | `main.cpp:72,1139` | `main.cpp:1171` |
| `handle_axis1` | `Handle` | `main.cpp:75` | `main.cpp:75` | `main.cpp:75,117,120,128,152,573,576,592,594,600,619,620,622,627,701,713,935,2201,2341,2343` |
| `handle_axis6` | `Handle` | `main.cpp:76` | `main.cpp:76` | `main.cpp:76,118,121,127,153,581,584,605,607,613,619,620,623,628,702,714,2200,2342,2344` |
| `ads` | `CADSComm` | `main.cpp:77` | `main.cpp:77` | `main.cpp:77,96,149` |
| `axis1_net_distance` | `HandleNetDistance` | `main.cpp:78` | `main.cpp:78` | `main.cpp:78,539,1283,2178,2460` |
| `axis6_net_distance` | `HandleNetDistance` | `main.cpp:78` | `main.cpp:78` | `main.cpp:78,540,1286,2180,2461` |
| `experiment_recorder` | `ExperimentRecorder` | `main.cpp:79` | `main.cpp:79` | `main.cpp:79,83,84,449,452,2090,2091,2093,2221,2224,2260,2290,2384` |
| `ads_snapshot_sequence` | `std::uint64_t` | `main.cpp:80` | `main.cpp:80,345,431,463` | `main.cpp:83,328,460,521,1984,1991` |
| `handle_velocity_recorder` | `HandleVelocityRecorder` | `main.cpp:86` | `main.cpp:86` | `main.cpp:86,88` |
| `handle_velocity_path` | `const auto` | `main.cpp:87` | `main.cpp:87` | `main.cpp:88,89` |
| `zenoh_service` | `ZenohService` | `main.cpp:92` | `main.cpp:92` | `main.cpp:92,111,2473,2480,2678` |
| `axis1_handle_filter` | `HandleFilterState` | `main.cpp:93` | `main.cpp:93,1064` | `main.cpp:93,154,678,680,682,900,1066,1301,1302,2215,2217` |
| `axis6_handle_filter` | `HandleFilterState` | `main.cpp:94` | `main.cpp:94,1065` | `main.cpp:94,155,679,681,683,905,1067,1303,1304,2216,2218` |
| `ads_communication` | `AdsCommunicationService` | `main.cpp:96` | `main.cpp:96` | `main.cpp:96,97,98,164,333,414,459,469,470,471,541,542,781,829,841,862,863,880,1106,1139,1141,1144,1171,1202,1203,1996,2193,2517,2519` |
| `arm_manual_ads` | `ArmManualAdsService` | `main.cpp:97` | `main.cpp:97` | `main.cpp:97,102,2406,2579,2582,2586,2589,2592,2596,2600,2604` |
| `axis1_input_handle` | `Handle* const` | `main.cpp:120` | `main.cpp:120` | `main.cpp:150,678,901,902,933,1064` |
| `axis6_input_handle` | `Handle* const` | `main.cpp:121` | `main.cpp:121` | `main.cpp:151,679,906,907,934,1065` |
| `ff` | `ForceFeedbackState` | `main.cpp:122` | `main.cpp:122,136,2570` | `main.cpp:122,135,208,352,800,1434,1435,1436,1437,2002,2003,2093,2137,2198,2254,2255,2256,2257,2258,2259,2356,2357,2358,2366,2367,2368,2369,2372,2373,2449,2450,2451,2565,2571,2573` |
| `pos` | `double[7]` | `main.cpp:141` | `main.cpp:141,1297,1298,1475,1572,1573,1574,1630,1631,1632,1792,1793,1797,1798,2024,2025,2026,2027` | `main.cpp:158,531,1795,1846,2157,2189` |
| `plc_act_pos` | `double[7]` | `main.cpp:142` | `main.cpp:142` | `main.cpp:159,479,480,482,483,484,485,490,752,753,756,771,772,1094,1095,1306,1307,1308,1329,1400,1401,1402,1403,1410,1418,1514,1515,1516,1517,1518,1519,1900,1901,1902,2002,2003,2024,2025,2205,2206,2310` |
| `plc_init_pos` | `double[7]` | `main.cpp:143` | `main.cpp:143` | `main.cpp:160,531,1306,1307,1308,1329,1480,1482,1485,1731,1732,1792,1797,2002,2003,2205,2206` |
| `plc_leftlimit` | `double[7]` | `main.cpp:144` | `main.cpp:144` | `main.cpp:161,316,1325,1480,1482,1485,1503,1504,1654,1655,1733,1734,1735,1837,1853,1863,2319,2320` |
| `plc_act_pos_from_left` | `double[7]` | `main.cpp:145` | `main.cpp:145` | `main.cpp:162,2311` |
| `ctx` | `AppContext` | `main.cpp:148` | `main.cpp:148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,164,205,206,207,208,209,210,211,212,213,214,215,216,217,218` | `main.cpp:148,166,167,220,221,222,223,240,249,691,1367,1508,1643,1647,1677,1846,1847,1993,2318` |
| `axis3_base_rel` | `double` | `main.cpp:169` | `main.cpp:169,1784` | `main.cpp:209` |
| `axis5_base_rel` | `double` | `main.cpp:170` | `main.cpp:170,1785` | `main.cpp:210,1731` |
| `axis2_hold_rel` | `double` | `main.cpp:172` | `main.cpp:172,270,479,752,771,1094,1795` | `main.cpp:211,1297,2026` |
| `axis7_hold_rel` | `double` | `main.cpp:173` | `main.cpp:173,271,480,753,772,1095,1468` | `main.cpp:212,1298,1474,2026` |
| `axis1_prev_linear_filtered` | `double` | `main.cpp:175` | `main.cpp:175,680,1066,1886,2215` | `main.cpp:213,1310` |
| `axis6_prev_linear_filtered` | `double` | `main.cpp:176` | `main.cpp:176,681,1067,1887,2216` | `main.cpp:214,1331` |
| `axis1_prev_rot_filtered` | `double` | `main.cpp:177` | `main.cpp:177,682,1888,2217` | `main.cpp:215` |
| `axis6_prev_rot_filtered` | `double` | `main.cpp:178` | `main.cpp:178,683,1889,2218` | `main.cpp:216` |
| `axis1_follow_cmd_abs` | `double` | `main.cpp:179` | `main.cpp:179,272,1790` | `main.cpp:217,1729` |
| `axis6_follow_cmd_abs` | `double` | `main.cpp:180` | `main.cpp:180,273,1791` | `main.cpp:218,1730` |
| `axis4_axis6_coupling_last_ms` | `ULONGLONG` | `main.cpp:181` | `main.cpp:181,1044,1669,1720,1725,1894` | `main.cpp:1713,1716` |
| `axis4_axis6_coupling_active_prev` | `bool` | `main.cpp:182` | `main.cpp:182,1043,1668,1721,1726,1893` | `main.cpp:1713` |
| `axis1_delivery_mapping_active` | `bool` | `main.cpp:184` | `main.cpp:184,227,233,1781` | `main.cpp:226,234,1737,1780` |
| `axis1_delivery_mapping_progress_mm` | `double` | `main.cpp:185` | `main.cpp:185,228,235,1776` | `main.cpp:1742,1778` |
| `axis1_delivery_mapping_applied_extra_mm` | `double` | `main.cpp:186` | `main.cpp:186,229,236,1777` | — |
| `dual_reverse_active` | `bool` | `main.cpp:189` | `main.cpp:189,949` | `main.cpp:205,495,953` |
| `startup` | `StartupState` | `main.cpp:191` | `main.cpp:191,569,769,770,806,807,890,891,1153,1154,2657,2658` | `main.cpp:191,245,1014,1154,1158,1159,1208,1209,1232,1233,1245,1510,1524,2329,2362,2363,2653` |
| `cal_cfg` | `ForceCalibrationConfig` | `main.cpp:192` | `main.cpp:192` | `main.cpp:192,2132,2211,2238,2242,2374` |
| `cal_state` | `ForceCalibrationState` | `main.cpp:193` | `main.cpp:193,346,347,348,349,350,351` | `main.cpp:193,354,355,356,357,358,2129,2132,2139,2212,2235,2238,2242,2244,2245,2249,2250,2359` |
| `axis1_crawl` | `CrawlState` | `main.cpp:194` | `main.cpp:194,202,951,952,1700` | `main.cpp:194,206,1316,1321,1322,1323,1324,1352,1353,1640,1649,1679,1680,1696,1702,1732,1793,1794` |
| `axis6_crawl` | `CrawlState` | `main.cpp:195` | `main.cpp:195,1150,2655` | `main.cpp:195,207,1337,1362,1363,1467,1642` |
| `planned_return` | `HandoffCoordinator` | `main.cpp:196` | `main.cpp:196,474,475,476,477,482,483,484,485,488,1395,1397,1400,1401,1402,1403,1406,1414,1458,1459,1949,1954,1963,1975,1976,1982,1983,1984,1993,2195` | `main.cpp:196,311,472,481,486,488,492,493,494,518,519,520,525,527,529,651,652,654,655,668,674,749,761,799,803,946,956,992,993,1006,1009,1011,1041,1126,1149,1156,1198,1225,1242,1277,1345,1347,1364,1393,1394,1406,1414,1420,1422,1440,1501,1512,1665,1849,1941,1945,1947,1952,1956,1958,1961,1968,1970,1971,1973,1977,1980,1989,1991,1994,1996,1998,1999,2002,2003,2004,2012,2013,2017,2021,2024,2025,2027,2031,2033,2036,2038,2041,2044,2058,2156,2166,2184,2185,2186,2191,2194,2332,2333,2480,2634` |
| `next_handoff_id` | `std::uint32_t` | `main.cpp:197` | `main.cpp:197,1442` | `main.cpp:1442,1443,1444` |
| `next_handoff_send_token` | `std::uint64_t` | `main.cpp:198` | `main.cpp:198` | `main.cpp:1459,1976` |
| `pending_start_control` | `bool` | `main.cpp:199` | `main.cpp:199,2480,2634` | `main.cpp:2480` |
| `dual_handoff` | `DualHandoffRequest` | `main.cpp:200` | `main.cpp:200,1817,1821,1822,1823,1824,1825,1826` | `main.cpp:200,799,803,956,1006,1011,1126,1149,1430,1687,1689,1690,1691,1692,1694,1704,1707,1708,1815,1819,1822,1823,1826,1827,1829,1830,1831,1834,1835,1836,1838,1839,1841,1861,1878,1882,1971,2017,2058,2166` |
| `spacing_recovery` | `SpacingRecoveryState` | `main.cpp:201` | `main.cpp:201,968,974,1494,1496,1497,1514,1515,1516,1517,1518,1519,1520,1551,1554,1555,1556,1557,1558,1578,1581,1583,1590,1595,1600,1603,1612,1613,1614,1615,1616,1617,1621,1625` | `main.cpp:201,543,545,547,549,733,736,738,740,801,802,947,954,989,990,1002,1009,1034,1111,1113,1114,1147,1148,1200,1201,1225,1226,1242,1276,1480,1482,1485,1491,1492,1501,1526,1527,1535,1536,1538,1542,1543,1565,1572,1573,1574,1576,1579,1587,1589,1606,1608,1619,1622,1630,1631,1632,1898,1900,1901,1902,1907,1911,1918,1919,2057,2072,2143,2202,2375,2376,2377,2635,2645,2646` |
| `control_active` | `bool` | `main.cpp:242` | `main.cpp:242,567,768,805,929,1081,1128,1159,1165,1209,1233,1250,1921,1966,2011,2660` | `main.cpp:566,767,793,804,928,962,983,1001,1007,1080,1127,1158,1164,1208,1232,1241,1249,1273,1294,1511,1920,1965,2010,2066,2141,2155,2202,2330,2635,2659` |
| `self_check_done` | `bool` | `main.cpp:276` | `main.cpp:276,502` | `main.cpp:279,507,556,563,564,648,812,1122,1124,1134,1170,1188,1195,1244,2334,2354,2642` |
| `has_self_check_flag` | `bool` | `main.cpp:277` | `main.cpp:277,562` | `main.cpp:559,1122,2642` |
| `last_self_check_done` | `bool` | `main.cpp:279` | `main.cpp:279,563,889,1195` | `main.cpp:1122,1124,1134` |
| `handle_reinit_req` | `bool` | `main.cpp:280` | `main.cpp:280,509` | `main.cpp:1198,1212` |
| `last_handle_reinit_req` | `bool` | `main.cpp:281` | `main.cpp:281,1212` | `main.cpp:1198` |
| `estop_hold_req` | `bool` | `main.cpp:282` | `main.cpp:282,510` | `main.cpp:512,1074,1076` |
| `estop_hold_active` | `bool` | `main.cpp:283` | `main.cpp:283,1079,1093` | `main.cpp:512,962,965,987,1000,1008,1033,1074,1099,1110,1116,1155,1204,1221,1245,1275,1293,1909,1940,2053,2066,2142,2154,2204,2331,2338,2480,2640` |
| `axis1_reverse_pressed_prev` | `bool` | `main.cpp:284` | `main.cpp:284,1070` | `main.cpp:520,1056` |
| `axis6_effective_reverse_prev` | `bool` | `main.cpp:285` | `main.cpp:285,1071` | `main.cpp:520` |
| `axis4_manual_error_prev` | `bool` | `main.cpp:286` | `main.cpp:286,1936` | `main.cpp:1932` |
| `axis4_manual_error_id_prev` | `unsigned long` | `main.cpp:287` | `main.cpp:287,1937` | `main.cpp:1932` |
| `axis4_ui_forward_pressed` | `bool` | `main.cpp:288` | `main.cpp:288,1029,2609` | `main.cpp:1025,1036,2464` |
| `axis4_ui_reverse_pressed` | `bool` | `main.cpp:289` | `main.cpp:289,1030,2610` | `main.cpp:1025,1037,2464` |
| `y_valve_open` | `bool` | `main.cpp:290` | `main.cpp:290,2617` | `main.cpp:2048,2465,2618` |
| `injector_ui_direction` | `int[2]` | `main.cpp:291` | `main.cpp:291,1021,2625` | `main.cpp:1017,1051,1053,2463` |
| `injector_ui_jog_deadline_ms` | `ULONGLONG[2]` | `main.cpp:292` | `main.cpp:292,1022,2626` | `main.cpp:1018,1019` |
| `axis4_ui_jog_deadline_ms` | `ULONGLONG` | `main.cpp:295` | `main.cpp:295,1031,2611` | `main.cpp:1026,1027` |
| `manual_jog_lease_ms` | `constexpr ULONGLONG` | `main.cpp:296` | `main.cpp:296` | `main.cpp:2613,2628` |
| `return_ads_fault_hold` | `bool` | `main.cpp:297` | `main.cpp:297,1231,1966,2011` | `main.cpp:926,962,966,986,999,1008,1110,1215,1241,1274,1293,1909,2154,2203,2337,2469,2480,2639` |
| `return_recovery` | `ReturnRecovery` | `main.cpp:298` | `main.cpp:298` | `main.cpp:298,492,1228,1234,1235,1427,1634,1636,1637,1638,1661,1964,2005,2009,2337` |
| `interrupted_return_mode` | `PlannedReturnMode` | `main.cpp:299` | `main.cpp:299,496,1428` | `main.cpp:1644,1658` |
| `interrupted_return_scope` | `PlannedReturnRebaseScope` | `main.cpp:300` | `main.cpp:300,1429` | `main.cpp:1658` |
| `original_return_intent` | `DualHandoffRequest` | `main.cpp:301` | `main.cpp:301,493,494,495,1430,1431,1432` | `main.cpp:301,1639,1651,1652` |
| `resuming_return` | `bool` | `main.cpp:302` | `main.cpp:302,1657,1660` | `main.cpp:1425` |
| `recovery_event_text` | `std::string` | `main.cpp:303` | `main.cpp:303,622,624,697,703,705,851,868,887,893,1235` | `main.cpp:303,624,706,754,853,869,894,1236,2352` |
| `force_sample` | `ForceSampleFrame` | `main.cpp:304` | `main.cpp:304,2110,2114` | `main.cpp:304,2117,2129,2132,2140,2199,2364,2365,2392` |
| `loop_count` | `int` | `main.cpp:305` | `main.cpp:305` | `main.cpp:925,2370` |
| `force_feedback_diag_reason` | `std::string` | `main.cpp:306` | `main.cpp:306,2149` | `main.cpp:306,2145` |
| `force_sample_diag_reason` | `std::string` | `main.cpp:307` | `main.cpp:307,2121` | `main.cpp:307,2120,2125` |
| `remote_force_zero_ok_count` | `std::uint32_t` | `main.cpp:362` | `main.cpp:362` | `main.cpp:367,2466` |
| `remote_force_zero_fail_count` | `std::uint32_t` | `main.cpp:363` | `main.cpp:363` | `main.cpp:368,2467` |
| `initial_sync_done` | `bool` | `main.cpp:372` | `main.cpp:372,766` | `main.cpp:673,689,710,714,744,759,1243,1273,2314,2468,2480,2639` |
| `plc_app_name_read` | `bool` | `main.cpp:373` | `main.cpp:373,786,797` | `main.cpp:778,812,1170` |
| `handle1_next_retry_ms` | `ULONGLONG` | `main.cpp:374` | `main.cpp:374,575,601` | `main.cpp:573` |
| `handle6_next_retry_ms` | `ULONGLONG` | `main.cpp:375` | `main.cpp:375,583,614` | `main.cpp:581` |
| `handle_soft_hold_active` | `bool` | `main.cpp:376` | `main.cpp:376,619` | `main.cpp:674,676,689,710,746,761,898,1224,1244,1274,1293,1908,1939,2203,2470,2639` |
| `connection_hold_active_prev` | `bool` | `main.cpp:377` | `main.cpp:377,729` | `main.cpp:673,727,728` |
| `cylinder_manual_mode` | `CylinderManualMode[4]` | `main.cpp:384` | `main.cpp:384,401,1549,2537,2549` | `main.cpp:399,1263,2076,2455` |
| `cylinder_manual_position` | `unsigned short[4]` | `main.cpp:390` | `main.cpp:390,2548` | `main.cpp:2077` |
| `cylinder_manual_restore` | `unsigned short[4]` | `main.cpp:391` | `main.cpp:391,420` | `main.cpp:2080` |
| `cylinder_manual_written_mask` | `std::uint8_t` | `main.cpp:392` | `main.cpp:392,396,421` | `main.cpp:408,2068,2079` |
| `spacing_recovery_button_pressed_prev` | `bool` | `main.cpp:424` | `main.cpp:424,977` | `main.cpp:941,943` |
| `pending_selfcheck_target` | `double[7]` | `main.cpp:426` | `main.cpp:426,2489` | `main.cpp:2497,2498,2499,2500,2518` |
| `pending_selfcheck_axes` | `unsigned int` | `main.cpp:427` | `main.cpp:427,2490,2507,2637` | `main.cpp:2501` |
| `selfcheck_command_error` | `bool` | `main.cpp:428` | `main.cpp:428,508,2491,2512,2521,2525` | `main.cpp:2354` |
| `selfcheck_start_pending` | `bool` | `main.cpp:429` | `main.cpp:429,506,2526` | `main.cpp:2055,2355,2508` |
| `manual_cylinder_override_active` | `bool` | `main.cpp:430` | `main.cpp:430,1260,1265` | `main.cpp:1278` |
| `handled_plc_restart_count` | `std::uint64_t` | `main.cpp:432` | `main.cpp:432,798` | `main.cpp:790` |
| `handled_reconnect_count` | `std::uint64_t` | `main.cpp:433` | `main.cpp:433,538` | `main.cpp:536` |
| `ads_soft_hold_active` | `bool` | `main.cpp:434` | `main.cpp:434,635,671,694` | `main.cpp:633,668,673,693,710,711,745,985,998,1156,1205,1245,1274,1293,1908,2112,2203,2471,2641` |
| `drained_ads_snapshots` | `std::vector<AdsFastSnapshot>` | `main.cpp:435` | `main.cpp:435` | `main.cpp:435,469,2225` |
| `handle_record_history` | `std::deque<HandleRecordSnapshot>` | `main.cpp:442` | `main.cpp:442` | `main.cpp:442,917,919,920,922,2274` |
| `vis_qpc_frequency` | `LARGE_INTEGER` | `main.cpp:443` | `main.cpp:443` | `main.cpp:443,444,918,2298` |
| `next_vis_publish_qpc` | `std::int64_t` | `main.cpp:445` | `main.cpp:445,2299,2302,2305` | `main.cpp:2299,2300,2303` |
| `recording_info` | `ExperimentStartInfo` | `main.cpp:446` | `main.cpp:446,447,448` | `main.cpp:446,449` |
| `event_hold_sources` | `int` | `main.cpp:454` | `main.cpp:454,724` | `main.cpp:718,720,723` |
| `event_connection_hold_active` | `bool` | `main.cpp:455` | `main.cpp:455,719` | `main.cpp:715,717` |
| `next_estop_recover_req_ms` | `ULONGLONG` | `main.cpp:1101` | `main.cpp:1101,1105` | `main.cpp:1103` |
