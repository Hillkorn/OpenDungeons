include(BoostTestTargets)

add_boost_test(Random
	SOURCES
	test_Random.cpp
        "${SRC}/utils/Random.h"
        "${SRC}/utils/Random.cpp")

add_boost_test(ODPacket
        SOURCES
        test_ODPacket.cpp
        "${SRC}/network/ODPacket.h"
        "${SRC}/network/ODPacket.cpp"
        LIBRARIES
        ${SFML_LIBRARIES})

add_boost_test(ConsoleInterface
        SOURCES
        test_ConsoleInterface.cpp
        "${SRC}/modes/AbstractModeManager.h"
        "${SRC}/modes/ConsoleInterface.h"
        "${SRC}/modes/ConsoleInterface.cpp"
        "${SRC}/modes/Command.h"
        "${SRC}/modes/Command.cpp")
