add_library(Skills STATIC
    AgentSkill.hpp
    SkillsLoader.hpp SkillsLoader.cpp
    SkillsManager.hpp SkillsManager.cpp
)

target_link_libraries(Skills
    PUBLIC
    Qt::Core
)

target_include_directories(Skills PUBLIC ${CMAKE_SOURCE_DIR})
