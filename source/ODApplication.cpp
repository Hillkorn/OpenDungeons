/*! \file   ODApplication.cpp
 *  \author Ogre team, andrewbuck, oln, StefanP.MUC
 *  \date   07 April 2011
 *  \brief  Class ODApplication containing everything to start the game
 *
 *  Copyright (C) 2011-2015  OpenDungeons Team
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ODApplication.h"

#include "render/ODFrameListener.h"
#include "gamemap/GameMap.h"
#include "render/TextRenderer.h"
#include "render/RenderManager.h"
#include "sound/MusicPlayer.h"
#include "sound/SoundEffectsManager.h"
#include "render/Gui.h"
#include "utils/ResourceManager.h"
#include "gamemap/MiniMap.h"
#include "utils/LogManager.h"
#include "camera/CameraManager.h"
#include "scripting/ASWrapper.h"
#include "modes/Console.h"
#include "gamemap/GameMap.h"
#include "utils/Random.h"
#include "utils/ConfigManager.h"
#include "entities/MapLight.h"
#include "network/ODServer.h"
#include "network/ODClient.h"

#include <OgreErrorDialog.h>
#include <OgreGpuProgramManager.h>
#include <OgreResourceGroupManager.h>
#include <OgreRoot.h>
#include <Overlay/OgreOverlaySystem.h>
#include <RTShaderSystem/OgreShaderGenerator.h>

#include <string>
#include <sstream>
#include <fstream>

ODApplication::ODApplication() :
    mRoot(nullptr),
    mWindow(nullptr),
    mInitialized(false)
{
    try
    {
        Random::initialize();

        ResourceManager* resMgr = new ResourceManager;

        std::cout << "Creating OGRE::Root instance; Plugins path: " << resMgr->getPluginsPath()
                  << "; config file: " << resMgr->getCfgFile()
                  << "; log file: " << resMgr->getLogFile() << std::endl;

        mRoot = new Ogre::Root(resMgr->getPluginsPath(),
                               resMgr->getCfgFile(),
                               resMgr->getLogFile());

        resMgr->setupOgreResources();

        LogManager* logManager = new LogManager();
        logManager->setLogDetail(Ogre::LL_BOREME);
        new ConfigManager;

        /* TODO: Skip this and use root.restoreConfig()
         * to load configuration settings if we are sure there are valid ones
         * saved in ogre.cfg
         * We should use this later (when we have an own setup options screen)
         * to avoid having the setup dialog started on every run
         */
        /* TODO: create our own options menu and define good default values
         *       (drop smaller than 800x600, AA, shadow quality, mipmaps, etc)
         */
        if (!mRoot->showConfigDialog())
            return;

        // Needed for the TextRenderer and the Render Manager
        mOverlaySystem = new Ogre::OverlaySystem();

        mWindow = mRoot->initialise(true, "OpenDungeons " + VERSION);

        new ODServer();
        new ODClient();

        //NOTE: This is currently done here as it has to be done after initialising mRoot,
        //but before running initialiseAllResourceGroups()
        Ogre::GpuProgramManager& gpuProgramManager = Ogre::GpuProgramManager::getSingleton();
        Ogre::ResourceGroupManager& resourceGroupManager = Ogre::ResourceGroupManager::getSingleton();
        if(gpuProgramManager.isSyntaxSupported("glsl"))
        {
            //Add GLSL shader location for RTShader system
            resourceGroupManager.addResourceLocation(
                        "materials/RTShaderLib/GLSL", "FileSystem", "Graphics");
            //Use patched version of shader on shader version 130+ systems
            Ogre::uint16 shaderVersion = mRoot->getRenderSystem()->getNativeShadingLanguageVersion();
            logManager->logMessage("Shader version is: " + Ogre::StringConverter::toString(shaderVersion));
            if(shaderVersion >= 130)
            {
                resourceGroupManager.addResourceLocation("materials/RTShaderLib/GLSL/130", "FileSystem", "Graphics");
            }
            else
            {
                resourceGroupManager.addResourceLocation("materials/RTShaderLib/GLSL/120", "FileSystem", "Graphics");
            }
        }

        //Initialise RTshader system
        // IMPORTANT: This needs to be initialized BEFORE the resource groups.
        // eg: Ogre::ResourceGroupManager::getSingletonPtr()->initialiseAllResourceGroups();
        // but after the render window, eg: mRoot->initialise();
        // This advice was taken from here:
        // http://www.ogre3d.org/forums/viewtopic.php?p=487445#p487445
        if (!Ogre::RTShader::ShaderGenerator::initialize())
        {
            //TODO - exit properly
            LogManager::getSingletonPtr()->logMessage("FATAL:"
                    "Failed to initialize the Real Time Shader System, exiting", Ogre::LML_CRITICAL);
            exit(1);
        }

        Ogre::ResourceGroupManager::getSingletonPtr()->initialiseAllResourceGroups();
        new MusicPlayer();
        new SoundEffectsManager();

        new Gui();
        TextRenderer* textRenderer = new TextRenderer();
        textRenderer->addTextBox("DebugMessages", ODApplication::MOTD.c_str(), 140,
                                    30, 50, 70, Ogre::ColourValue::Green);
        textRenderer->addTextBox(ODApplication::POINTER_INFO_STRING, "",
                                    0, 0, 200, 50, Ogre::ColourValue::White);

        mFrameListener = new ODFrameListener(mWindow, mOverlaySystem);
        mRoot->addFrameListener(mFrameListener);

        mRoot->startRendering();

        mInitialized = true;
    }
    catch(const Ogre::Exception& e)
    {
        std::cerr << "An internal Ogre3D error ocurred: " << e.getFullDescription() << std::endl;
        displayErrorMessage("Internal Ogre3D exception: " + e.getFullDescription());
    }
}

ODApplication::~ODApplication()
{
    cleanUp();
}

void ODApplication::displayErrorMessage(const std::string& message, bool log)
{
    if (log)
    {
        LogManager::getSingleton().logMessage(message, Ogre::LML_CRITICAL);
    }
    Ogre::ErrorDialog e;
    e.display(message, LogManager::GAMELOG_NAME);
}

void ODApplication::cleanUp()
{
    LogManager* logManager = LogManager::getSingletonPtr();
    logManager->logMessage("Quitting main application...", Ogre::LML_CRITICAL);

    if (mInitialized)
    {
        logManager->logMessage("Stopping server...");
        ODServer::getSingleton().stopServer();

        logManager->logMessage("Disconnecting client...");
        ODClient::getSingleton().disconnect();

        logManager->logMessage("Removing ODFrameListener from root...");
        mRoot->removeFrameListener(mFrameListener);
        logManager->logMessage("Deleting ODFrameListener...");
        delete mFrameListener;
        logManager->logMessage("Deleting Ogre::OverlaySystem...");
        delete mOverlaySystem;
        logManager->logMessage("Deleting MusicPlayer...");
        delete MusicPlayer::getSingletonPtr();
        logManager->logMessage("Deleting TextRenderer...");
        delete TextRenderer::getSingletonPtr();
        logManager->logMessage("Deleting Gui...");
        delete Gui::getSingletonPtr();
        logManager->logMessage("Deleting SoundEffectsManager...");
        delete SoundEffectsManager::getSingletonPtr();
        logManager->logMessage("Deleting ODServer...");
        delete ODServer::getSingletonPtr();
        logManager->logMessage("Deleting ODClient...");
        delete ODClient::getSingletonPtr();
    }

    logManager->logMessage("Deleting ConfigManager...");
    delete ConfigManager::getSingletonPtr();
    logManager->logMessage("Deleting ResourceManager...");
    delete ResourceManager::getSingletonPtr();
    logManager->logMessage("Deleting LogManager and Ogre::Root...");
    delete LogManager::getSingletonPtr();
    delete mRoot;

    mInitialized = false;
}

//TODO: find some better places for some of these
const double ODApplication::DEFAULT_FRAMES_PER_SECOND = 60.0;
double ODApplication::MAX_FRAMES_PER_SECOND = DEFAULT_FRAMES_PER_SECOND;
double ODApplication::turnsPerSecond = 1.4;
#ifdef OD_VERSION
const std::string ODApplication::VERSION = OD_VERSION;
#else
const std::string ODApplication::VERSION = "undefined";
#endif
const std::string ODApplication::VERSIONSTRING = "OpenDungeons_Version:" + VERSION;
std::string ODApplication::MOTD = "Welcome to Open Dungeons\tVersion:  " + VERSION;
const std::string ODApplication::POINTER_INFO_STRING = "pointerInfo";
