project(translations)
set(TRS
	tiled_ru.ts
	tiled_en.ts
	tiled_lv.ts
	tiled_it.ts
	tiled_zh.ts
	tiled_fr.ts
	tiled_cs.ts
	tiled_pt.ts
	tiled_nl.ts
	tiled_pt_BR.ts
	tiled_he.ts
	tiled_es.ts
	tiled_de.ts
	tiled_ja.ts
)
QT4_ADD_TRANSLATION(_qm_files ${TRS})

add_custom_target(translations_target SOURCES ${_qm_files})
add_dependencies(TileZed translations_target)

if (NOT EXISTS ${CMAKE_BINARY_DIR}/Debug/translations)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/Debug/translations)
endif ()

foreach (_qm ${_qm_files})
    get_filename_component (_qm_name "${_qm}" NAME)
    add_custom_command (TARGET translations_target POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E
                        copy_if_different
                          ${_qm}
                          ${CMAKE_BINARY_DIR}/Debug/translations/${_qm_name})
endforeach ()

#--------------------------------------------------------------------------------
# install translations from build directory

install(FILES ${_qm_files} DESTINATION translations)

#--------------------------------------------------------------------------------
# install translations from Qt directory

set(qt_trs qt_cs.qm qt_de.qm qt_es.qm qt_fr.qm qt_he.qm qt_ja.qm qt_pt.qm qt_ru.qm qt_zh_CN.qm qt_zh_TW.qm)
foreach(name ${qt_trs})
	list(APPEND _qt_qm_files ${QT_TRANSLATIONS_DIR}/${name})
endforeach()
install(FILES ${_qt_qm_files} DESTINATION translations)

