add_llvm_library(ObjCDirectFinder MODULE ObjCDirectFinder.cpp PLUGIN_TOOL clang)

set(LLVM_LINK_COMPONENTS
	Support
)

if(LLVM_ENABLE_PLUGINS AND (WIN32 OR CYGWIN))
	clang_target_link_libraries(ObjCDirectFinder PRIVATE
		clangAST
		clangBasic
		clangFrontend
	)	
endif()