#pragma once

#include "Config.hpp"
#include "Common.hpp"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>

struct GLFWwindow;

/**
 \brief Performs system basic operations, file picking, interface setup and loop.
 \ingroup Helpers
 */
namespace System {
	
	/**
	 \brief GUI helpers, internally backed by ImGUI.
	 \ingroup Helpers
	 */
	namespace GUI {
	
		/** Start registering GUI items. */
		void beginFrame();
	
		/** Finish registering GUI items and render them. */
		void endFrame();
		
		/** Clean internal resources. */
		void clean();
	
	}
	
	/** Create a new window backed by an OpenGL context.
	 \param name the name of the window
	 \param config the configuration to use (additional info will be added to it)
	 \return a pointer to the OS window
	 */
	GLFWwindow* initWindow(const std::string & name, RenderingConfig & config);
	
	/** System actions that can be executed by the window. */
	enum class Action {
		None, ///< Do nothing.
		Quit, ///< Quit the application.
		Fullscreen, ///< Switch the window from/to fullscreen mode.
		Vsync ///< Switch the v-sync on/off.
	};
	
	/** Execute an action related to the windowing system.
	 \param window the window
	 \param config the current window configuration
	 \param action the system action to perform
	 */
	void performWindowAction(GLFWwindow * window, RenderingConfig & config, const Action action);
	
	
	/** The file picker mode. */
	enum class Picker {
		Load, ///< Load an existing file.
		Directory, ///< open or create a directory.
		Save ///< Save to a new or existing file.
	};
	
	/** Present a filesystem document picker to the user, using native controls.
	 \param mode the type of item to ask to the user (load, save, directory)
	 \param startDir the initial directory when the picker is opended
	 \param outPath the path to the item selected by the user
	 \param extensions (optional) the extensions allowed, separated by "," or ";"
	 \return true if the user picked an item, false if cancelled.
	 */
	bool showPicker(const Picker mode, const std::string & startDir, std::string & outPath, const std::string & extensions = "");
	
	/** Create a directory.
	 \param directory the path to the directory to create
	 \return true if the creation is successful.
	 \note If the directory already exists, it will fail.
	 \warning This function will not create intermediate directories.
	 */
	bool createDirectory(const std::string & directory);
	
}
