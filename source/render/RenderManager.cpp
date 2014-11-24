/*!
 *  \file   RenderManager.cpp
 *  \date   26 March 2001
 *  \author oln, paul424
 *  \brief  handles the render requests
 *  Copyright (C) 2011-2014  OpenDungeons Team
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

#include "render/RenderManager.h"

#include "gamemap/GameMap.h"
#include "render/RenderRequest.h"
#include "network/ODServer.h"
#include "rooms/Room.h"
#include "entities/RenderedMovableEntity.h"
#include "entities/MapLight.h"
#include "entities/Creature.h"
#include "entities/Weapon.h"
#include "traps/Trap.h"
#include "game/Player.h"
#include "utils/ResourceManager.h"
#include "game/Seat.h"
#include "gamemap/MapLoader.h"
#include "entities/MovableGameEntity.h"

#include "utils/LogManager.h"
#include "entities/GameEntity.h"

#include <OgreMesh.h>
#include <OgreBone.h>
#include <OgreSkeleton.h>
#include <OgreSkeletonInstance.h>
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgreSubEntity.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreMovableObject.h>
#include <OgreEntity.h>
#include <OgreSubMesh.h>
#include <OgreCompositorManager.h>
#include <OgreViewport.h>
#include <OgreRoot.h>
#include <Overlay/OgreOverlaySystem.h>

//#include <RTShaderSystem/OgreShaderGenerator.h>
#include <RTShaderSystem/OgreShaderExPerPixelLighting.h>
#include <RTShaderSystem/OgreShaderExNormalMapLighting.h>
#include <sstream>

using std::stringstream;

template<> RenderManager* Ogre::Singleton<RenderManager>::msSingleton = 0;

const Ogre::Real RenderManager::BLENDER_UNITS_PER_OGRE_UNIT = 10.0;

RenderManager::RenderManager(Ogre::OverlaySystem* overlaySystem) :
    mGameMap(NULL),
    mViewport(NULL),
    mShaderGenerator(NULL),
    mInitialized(false)
{
    // Use Ogre::SceneType enum instead of string to identify the scene manager type; this is more robust!
    mSceneManager = Ogre::Root::getSingleton().createSceneManager(Ogre::ST_INTERIOR, "SceneManager");
    mSceneManager->addRenderQueueListener(overlaySystem);

    mRockSceneNode = mSceneManager->getRootSceneNode()->createChildSceneNode("Rock_scene_node");
    mCreatureSceneNode = mSceneManager->getRootSceneNode()->createChildSceneNode("Creature_scene_node");
    mRoomSceneNode = mSceneManager->getRootSceneNode()->createChildSceneNode("Room_scene_node");
    mLightSceneNode = mSceneManager->getRootSceneNode()->createChildSceneNode("Light_scene_node");
}

RenderManager::~RenderManager()
{
}

void RenderManager::triggerCompositor(const std::string& compositorName)
{
    Ogre::CompositorManager::getSingleton().setCompositorEnabled(mViewport, compositorName.c_str(), true);
}

void RenderManager::createScene(Ogre::Viewport* nViewport)
{
    LogManager::getSingleton().logMessage("Creating scene...", Ogre::LML_NORMAL);

    mViewport = nViewport;

    //Set up the shader generator
    mShaderGenerator = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
    //shaderGenerator->setTargetLanguage("glsl");
    Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
        ResourceManager::getSingleton().getShaderCachePath(), "FileSystem", "Graphics");

    //FIXME - this is a workaround for an issue where the shader cache files are not found.
    //Haven't found out why this started happening. Think it worked in 3faa1aa285df504350f9704bdf20eb851fc5be3d
    //atleast.
    Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
        ResourceManager::getSingleton().getShaderCachePath() + "../", "FileSystem", "Graphics");
    mShaderGenerator->setShaderCachePath(ResourceManager::getSingleton().getShaderCachePath());

    mShaderGenerator->addSceneManager(mSceneManager);

    mViewport->setMaterialScheme(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);

    rtssTest();

    // Sets the overall world lighting.
    mSceneManager->setAmbientLight(Ogre::ColourValue(0.3, 0.3, 0.3));

    // Create the scene nodes that will follow the mouse pointer.
    // Create the single tile selection mesh
    Ogre::Entity* ent = mSceneManager->createEntity("SquareSelector", "SquareSelector.mesh");
    Ogre::SceneNode* node = mSceneManager->getRootSceneNode()->createChildSceneNode("SquareSelectorNode");
    node->translate(Ogre::Vector3(0, 0, 0));
    node->scale(Ogre::Vector3(BLENDER_UNITS_PER_OGRE_UNIT,
                              BLENDER_UNITS_PER_OGRE_UNIT, 0.45 * BLENDER_UNITS_PER_OGRE_UNIT));
    node->attachObject(ent);
    Ogre::SceneNode *node2 = node->createChildSceneNode("Hand_node");
    node2->setPosition((Ogre::Real)(0.0 / BLENDER_UNITS_PER_OGRE_UNIT),
                       (Ogre::Real)(0.0 / BLENDER_UNITS_PER_OGRE_UNIT),
                       (Ogre::Real)(3.0 / BLENDER_UNITS_PER_OGRE_UNIT));
    node2->scale(Ogre::Vector3((Ogre::Real)(1.0 / BLENDER_UNITS_PER_OGRE_UNIT),
                               (Ogre::Real)(1.0 / BLENDER_UNITS_PER_OGRE_UNIT),
                               (Ogre::Real)(1.0 / BLENDER_UNITS_PER_OGRE_UNIT)));

    // Create the light which follows the single tile selection mesh
    Ogre::Light* light = mSceneManager->createLight("MouseLight");
    light->setType(Ogre::Light::LT_POINT);
    light->setDiffuseColour(Ogre::ColourValue(0.65, 0.65, 0.45));
    light->setSpecularColour(Ogre::ColourValue(0.65, 0.65, 0.45));
    light->setPosition(0, 0, 6);
    light->setAttenuation(50, 1.0, 0.09, 0.032);

    LogManager::getSingleton().logMessage("Creating compositor...", Ogre::LML_NORMAL);
    Ogre::CompositorManager::getSingleton().addCompositor(mViewport, "B&W");
}

void RenderManager::processRenderRequests()
{
    while (!mRenderQueue.empty())
    {
        // Remove the first item from the render queue
        RenderRequest *curReq = mRenderQueue.front();
        mRenderQueue.pop_front();

        // Handle the request
        curReq->executeRequest(this);

        delete curReq;
        curReq = NULL;
    }
}

void RenderManager::queueRenderRequest_priv(RenderRequest* renderRequest)
{
    mRenderQueue.push_back(renderRequest);
}

void RenderManager::rrRefreshTile(Tile* curTile)
{
    int rt = 0;
    std::string tileName = curTile->getOgreNamePrefix() + curTile->getName();

    if (!mSceneManager->hasSceneNode(tileName + "_node"))
        return;

    // We keep visibility if the tile is refreshed
    Ogre::Entity* entity = mSceneManager->getEntity(tileName);
    bool visible = entity->getVisible();

    // Unlink and delete the old mesh
    mSceneManager->getSceneNode(tileName + "_node")->detachObject(tileName);
    mSceneManager->destroyEntity(tileName);

    std::string meshName = Tile::meshNameFromNeighbors(curTile->getType(),
                                                       curTile->getFullnessMeshNumber(),
                                                       mGameMap->getNeighborsTypes(curTile),
                                                       mGameMap->getNeighborsFullness(curTile),
                                                       rt);

    Ogre::Entity* ent = mSceneManager->createEntity(tileName, meshName);
    ent->setVisible(visible);

    if(curTile->getType() == Tile::gold && curTile->getFullness() > 0.0)
    {
        for(unsigned int ii = 0; ii < ent->getNumSubEntities(); ++ii)
        {
            ent->getSubEntity(ii)->setMaterialName("Gold");
        }
    }
    else if(curTile->getType() == Tile::rock)
    {
        for(unsigned int ii = 0; ii < ent->getNumSubEntities(); ++ii)
        {
            ent->getSubEntity(ii)->setMaterialName("Rock");
        }

    }
    else if(curTile->getType() == Tile::lava)
    {
        for(unsigned int ii = 0; ii < ent->getNumSubEntities(); ++ii)
        {
            Ogre::SubEntity* subEnt = ent->getSubEntity(ii);
            if (subEnt->getMaterialName() == "Water")
                subEnt->setMaterialName("Lava");
        }
    }

    colourizeEntity(ent, curTile->getSeat(), curTile->getMarkedForDigging(mGameMap->getLocalPlayer()));

    // Link the tile mesh back to the relevant scene node so OGRE will render it
    Ogre::SceneNode* node = mSceneManager->getSceneNode(tileName + "_node");
    node->attachObject(ent);
    node->resetOrientation();
    node->roll(Ogre::Degree((Ogre::Real)(-1 * rt * 90)));

    // Refresh visibility
    ent->setVisible(visible);
}


void RenderManager::rrCreateTile(Tile* curTile)
{
    int rt = 0;
    std::string meshName = Tile::meshNameFromNeighbors(curTile->getType(),
                                                       curTile->getFullnessMeshNumber(),
                                                       mGameMap->getNeighborsTypes(curTile),
                                                       mGameMap->getNeighborsFullness(curTile),
                                                       rt);

    Ogre::Entity* ent = mSceneManager->createEntity(curTile->getOgreNamePrefix() + curTile->getName(), meshName);

    if(curTile->getType() == Tile::gold)
    {
        for(unsigned int ii = 0; ii < ent->getNumSubEntities(); ++ii)
        {
            ent->getSubEntity(ii)->setMaterialName("Gold");
        }
    }
    else if(curTile->getType() == Tile::rock)
    {
        for(unsigned int ii = 0; ii < ent->getNumSubEntities(); ++ii)
        {
            ent->getSubEntity(ii)->setMaterialName("Rock");
        }
    }
    else if(curTile->getType() == Tile::lava)
    {
        for(unsigned int ii = 0; ii < ent->getNumSubEntities(); ++ii)
        {
            Ogre::SubEntity* subEnt = ent->getSubEntity(ii);
            if (subEnt->getMaterialName() == "Water")
                subEnt->setMaterialName("Lava");
        }
    }

    if (curTile->getType() == Tile::claimed)
    {
        colourizeEntity(ent, curTile->getSeat(), curTile->getMarkedForDigging(mGameMap->getLocalPlayer()));
    }

    Ogre::SceneNode* node = mSceneManager->getRootSceneNode()->createChildSceneNode(curTile->getOgreNamePrefix() + curTile->getName() + "_node");

    Ogre::MeshPtr meshPtr = ent->getMesh();
    unsigned short src, dest;
    if (!meshPtr->suggestTangentVectorBuildParams(Ogre::VES_TANGENT, src, dest))
    {
        meshPtr->buildTangentVectors(Ogre::VES_TANGENT, src, dest);
    }

    node->setPosition(static_cast<Ogre::Real>(curTile->x), static_cast<Ogre::Real>(curTile->y), 0);

    node->attachObject(ent);

    node->setScale(Ogre::Vector3((Ogre::Real)(4.0 / BLENDER_UNITS_PER_OGRE_UNIT),
                                 (Ogre::Real)(4.0 / BLENDER_UNITS_PER_OGRE_UNIT),
                                 (Ogre::Real)(5.0 / BLENDER_UNITS_PER_OGRE_UNIT)));
    node->resetOrientation();
    node->roll(Ogre::Degree((Ogre::Real)(-1 * rt * 90)));

    // Test whether the tile should be shown
    if (curTile->getCoveringRoom() != nullptr)
        ent->setVisible(curTile->getCoveringRoom()->shouldDisplayGroundTile());
    else if (curTile->getCoveringTrap() != nullptr)
        ent->setVisible(curTile->getCoveringTrap()->shouldDisplayGroundTile());
    else
        ent->setVisible(true);
}

void RenderManager::rrDestroyTile(Tile* curTile)
{
    if (mSceneManager->hasEntity(curTile->getOgreNamePrefix() + curTile->getName()))
    {
        Ogre::Entity* ent = mSceneManager->getEntity(curTile->getOgreNamePrefix() + curTile->getName());
        Ogre::SceneNode* node = mSceneManager->getSceneNode(curTile->getOgreNamePrefix() + curTile->getName() + "_node");
        node->detachAllObjects();
        mSceneManager->destroySceneNode(node->getName());
        mSceneManager->destroyEntity(ent);
    }
}

void RenderManager::rrTemporalMarkTile(Tile* curTile)
{
    Ogre::SceneManager* mSceneMgr = RenderManager::getSingletonPtr()->getSceneManager();
    Ogre::Entity* ent;
    std::stringstream ss;
    std::stringstream ss2;

    bool bb = curTile->getSelected();

    ss.str(std::string());
    ss << curTile->getOgreNamePrefix();
    ss << curTile->getName();
    ss << "_selection_indicator";

    if (mSceneMgr->hasEntity(ss.str()))
    {
        ent = mSceneMgr->getEntity(ss.str());
    }
    else
    {
        ss2.str(std::string());
        ss2 << curTile->getOgreNamePrefix();
        ss2 << curTile->getName();
        ss2 << "_node";
        ent = mSceneMgr->createEntity(ss.str(), "SquareSelector.mesh");
        Ogre::SceneNode* node = mSceneManager->getSceneNode(ss2.str())->createChildSceneNode(ss.str()+"Node");
        node->setInheritScale(false);
        node->scale(Ogre::Vector3(BLENDER_UNITS_PER_OGRE_UNIT,
                                  BLENDER_UNITS_PER_OGRE_UNIT, 0.45 * BLENDER_UNITS_PER_OGRE_UNIT));
        node->attachObject(ent);
    }

    ent->setVisible(bb);
}

void RenderManager::rrDetachTile(GameEntity* curEntity)
{
    Ogre::SceneNode* tileNode = mSceneManager->getSceneNode(curEntity->getOgreNamePrefix() + curEntity->getName() + "_node");

    curEntity->pSN=(tileNode->getParentSceneNode());
    curEntity->pSN->removeChild(tileNode);
}

void RenderManager::rrAttachTile(GameEntity* curEntity)
{
    Ogre::SceneNode* creatureNode = mSceneManager->getSceneNode(curEntity->getOgreNamePrefix() + curEntity->getName() + "_node");

    Ogre::SceneNode* parentNode = creatureNode->getParentSceneNode();
    if (parentNode == nullptr)
    {
        curEntity->pSN->addChild(creatureNode);
    }
    else
    {
        curEntity->pSN = parentNode;
    }
}

void RenderManager::rrDetachEntity(GameEntity* curEntity)
{
    Ogre::SceneNode* creatureNode = mSceneManager->getSceneNode(curEntity->getOgreNamePrefix() + curEntity->getName() + "_node");

    curEntity->pSN->removeChild(creatureNode);
}

void RenderManager::rrAttachEntity(GameEntity* curEntity)
{
    Ogre::SceneNode* entityNode = mSceneManager->getSceneNode(curEntity->getOgreNamePrefix() + curEntity->getName() + "_node");

    curEntity->pSN->addChild(entityNode);
}

void RenderManager::rrShowSquareSelector(const Ogre::Real& xPos, const Ogre::Real& yPos)
{
    mSceneManager->getEntity("SquareSelector")->setVisible(true);
    mSceneManager->getSceneNode("SquareSelectorNode")->setPosition(xPos, yPos, 0.0);
}

void RenderManager::rrCreateBuilding(Building* curBuilding, Tile* curTile)
{
    if (curBuilding->shouldDisplayBuildingTile())
    {
        std::stringstream tempSS;
        tempSS << curBuilding->getOgreNamePrefix() << curBuilding->getNameTile(curTile);
        // Create the room ground tile

        Ogre::Entity* ent = mSceneManager->createEntity(tempSS.str(), curBuilding->getMeshName() + ".mesh");
        Ogre::SceneNode* node = mRoomSceneNode->createChildSceneNode(tempSS.str() + "_node");

        node->setPosition(static_cast<Ogre::Real>(curTile->x),
                        static_cast<Ogre::Real>(curTile->y),
                        static_cast<Ogre::Real>(0.0f));
        node->setScale(Ogre::Vector3(BLENDER_UNITS_PER_OGRE_UNIT,
                                    BLENDER_UNITS_PER_OGRE_UNIT,
                                    BLENDER_UNITS_PER_OGRE_UNIT));
        node->attachObject(ent);
    }

    Ogre::Entity* tileEnt = mSceneManager->getEntity(curTile->getOgreNamePrefix() + curTile->getName());
    tileEnt->setVisible(curBuilding->shouldDisplayGroundTile());
}

void RenderManager::rrDestroyBuilding(Building* curBuilding, Tile* curTile)
{
    std::stringstream tempSS;
    tempSS << curBuilding->getOgreNamePrefix() << curBuilding->getNameTile(curTile);

    std::string tempString = tempSS.str();
    // Buildings do not necessarily use ground mesh. So, we remove it only if it exists
    if(!mSceneManager->hasEntity(tempString))
        return;

    Ogre::Entity* ent = mSceneManager->getEntity(tempString);
    Ogre::SceneNode* node = mSceneManager->getSceneNode(tempString + "_node");
    node->detachObject(ent);
    mRoomSceneNode->removeChild(node);
    mSceneManager->destroyEntity(ent);
    mSceneManager->destroySceneNode(node->getName());

    // Show the tile being under
    Ogre::Entity* tileEnt = mSceneManager->getEntity(curTile->getOgreNamePrefix() + curTile->getName());
    tileEnt->setVisible(true);
}

void RenderManager::rrCreateRenderedMovableEntity(RenderedMovableEntity* curRenderedMovableEntity)
{
    std::string meshName = curRenderedMovableEntity->getMeshName();
    std::string tempString = curRenderedMovableEntity->getOgreNamePrefix() + curRenderedMovableEntity->getName();

    Ogre::Entity* ent = mSceneManager->createEntity(tempString, meshName + ".mesh");
    Ogre::SceneNode* node = mRoomSceneNode->createChildSceneNode(tempString + "_node");

    node->setPosition(curRenderedMovableEntity->getPosition());
    node->setScale(Ogre::Vector3(0.7, 0.7, 0.7));
    node->roll(Ogre::Degree(curRenderedMovableEntity->getRotationAngle()));
    node->attachObject(ent);
    curRenderedMovableEntity->pSN = (node->getParentSceneNode());

    // If it is required, we hide the tile
    if(curRenderedMovableEntity->getHideCoveredTile())
    {
        Tile* posTile = curRenderedMovableEntity->getPositionTile();
        if(posTile == nullptr)
            return;

        std::string tileName = posTile->getOgreNamePrefix() + posTile->getName();
        if (!mSceneManager->hasEntity(tileName))
            return;

        Ogre::Entity* entity = mSceneManager->getEntity(tileName);
        entity->setVisible(false);
    }
}

void RenderManager::rrDestroyRenderedMovableEntity(RenderedMovableEntity* curRenderedMovableEntity)
{
    std::string tempString = curRenderedMovableEntity->getOgreNamePrefix()
                             + curRenderedMovableEntity->getName();
    Ogre::Entity* ent = mSceneManager->getEntity(tempString);
    Ogre::SceneNode* node = mSceneManager->getSceneNode(tempString + "_node");
    node->detachObject(ent);
    mSceneManager->destroySceneNode(node->getName());
    mSceneManager->destroyEntity(ent);

    // If it was hidden, we display the tile
    if(curRenderedMovableEntity->getHideCoveredTile())
    {
        Tile* posTile = curRenderedMovableEntity->getPositionTile();
        if(posTile == nullptr)
            return;

        std::string tileName = posTile->getOgreNamePrefix() + posTile->getName();
        if (!mSceneManager->hasEntity(tileName))
            return;

        Ogre::Entity* entity = mSceneManager->getEntity(tileName);
        if (posTile->getCoveringRoom() != nullptr)
            entity->setVisible(posTile->getCoveringRoom()->shouldDisplayGroundTile());
        else if (posTile->getCoveringTrap() != nullptr)
            entity->setVisible(posTile->getCoveringTrap()->shouldDisplayGroundTile());
        else
            entity->setVisible(true);
    }
}

void RenderManager::rrCreateCreature(Creature* curCreature)
{
    const std::string& meshName = curCreature->getDefinition()->getMeshName();
    const Ogre::Vector3& scale = curCreature->getDefinition()->getScale();

    assert(curCreature != 0);
    //assert(curCreature->getDefinition() != 0);

    // Load the mesh for the creature
    std::string creatureName = curCreature->getOgreNamePrefix() + curCreature->getName();
    Ogre::Entity* ent = mSceneManager->createEntity(creatureName, meshName);
    Ogre::MeshPtr meshPtr = ent->getMesh();

    unsigned short src, dest;
    if (!meshPtr->suggestTangentVectorBuildParams(Ogre::VES_TANGENT, src, dest))
    {
        meshPtr->buildTangentVectors(Ogre::VES_TANGENT, src, dest);
    }

    //Disabled temporarily for normal-mapping
    //colourizeEntity(ent, curCreature->color);
    Ogre::SceneNode* node = mCreatureSceneNode->createChildSceneNode(creatureName + "_node");
    curCreature->mSceneNode = node;
    node->setPosition(curCreature->getPosition());
    node->setScale(scale);
    node->attachObject(ent);
    curCreature->pSN = (node->getParentSceneNode());
    // curCreature->pSN->removeChild(node);
}

void RenderManager::rrDestroyCreature(Creature* curCreature)
{
    std::string creatureName = curCreature->getOgreNamePrefix() + curCreature->getName();
    if (mSceneManager->hasEntity(creatureName))
    {
        Ogre::Entity* ent = mSceneManager->getEntity(creatureName);
        Ogre::SceneNode* node = mSceneManager->getSceneNode(creatureName + "_node");
        node->detachObject(ent);
        mCreatureSceneNode->removeChild(node);
        mSceneManager->destroyEntity(ent);
        mSceneManager->destroySceneNode(node->getName());
    }
    curCreature->mSceneNode = NULL;
}

void RenderManager::rrOrientSceneNodeToward(MovableGameEntity* gameEntity, const Ogre::Vector3& direction)
{
    Ogre::SceneNode* node = mSceneManager->getSceneNode(gameEntity->getOgreNamePrefix() + gameEntity->getName() + "_node");
    Ogre::Vector3 tempVector = node->getOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Y;

    // Work around 180 degree quaternion rotation quirk
    if ((1.0f + tempVector.dotProduct(direction)) < 0.0001f)
    {
        node->roll(Ogre::Degree(180));
    }
    else
    {
        node->rotate(tempVector.getRotationTo(direction));
    }
}

void RenderManager::rrScaleSceneNode(Ogre::SceneNode* node, const Ogre::Vector3& scale)
{
    if (node != NULL)
    {
        node->scale(scale);
    }
}

void RenderManager::rrCreateWeapon(Creature* curCreature, const Weapon* curWeapon, const std::string& hand)
{
    Ogre::Entity* ent = mSceneManager->getEntity(curCreature->getOgreNamePrefix() + curCreature->getName());
    //colourizeEntity(ent, curCreature->color);
    Ogre::Entity* weaponEntity = mSceneManager->createEntity(curWeapon->getOgreNamePrefix()
                                 + hand + "_" + curCreature->getName(),
                                 curWeapon->getMeshName());
    Ogre::Bone* weaponBone = ent->getSkeleton()->getBone(
                                 curWeapon->getOgreNamePrefix() + hand);

    // Rotate by -90 degrees around the x-axis from the bone's rotation.
    Ogre::Quaternion rotationQuaternion;
    rotationQuaternion.FromAngleAxis(Ogre::Degree(-90.0), Ogre::Vector3(1.0,
                                     0.0, 0.0));

    ent->attachObjectToBone(weaponBone->getName(), weaponEntity,
                            rotationQuaternion);
}

void RenderManager::rrDestroyWeapon(Creature* curCreature, const Weapon* curWeapon, const std::string& hand)
{
     Ogre::Entity* ent = mSceneManager->getEntity(curWeapon->getOgreNamePrefix()
         + hand + "_" + curCreature->getName());
     mSceneManager->destroyEntity(ent);
}

void RenderManager::rrCreateMapLight(MapLight* curMapLight, bool displayVisual)
{
    // Create the light and attach it to the lightSceneNode.
    std::string mapLightName = curMapLight->getOgreNamePrefix() + curMapLight->getName();
    Ogre::Light* light = mSceneManager->createLight(mapLightName);
    light->setDiffuseColour(curMapLight->getDiffuseColor());
    light->setSpecularColour(curMapLight->getSpecularColor());
    light->setAttenuation(curMapLight->getAttenuationRange(),
                          curMapLight->getAttenuationConstant(),
                          curMapLight->getAttenuationLinear(),
                          curMapLight->getAttenuationQuadratic());

    // Create the base node that the "flicker_node" and the mesh attach to.
    Ogre::SceneNode* mapLightNode = mLightSceneNode->createChildSceneNode(mapLightName + "_node");
    mapLightNode->setPosition(curMapLight->getPosition());

    if (displayVisual)
    {
        // Create the MapLightIndicator mesh so the light can be drug around in the map editor.
        Ogre::Entity* lightEntity = mSceneManager->createEntity(MapLight::MAPLIGHT_INDICATOR_PREFIX
                                    + curMapLight->getName(), "Lamp.mesh");
        mapLightNode->attachObject(lightEntity);
    }

    // Create the "flicker_node" which moves around randomly relative to
    // the base node.  This node carries the light itself.
    Ogre::SceneNode* flickerNode = mapLightNode->createChildSceneNode(mapLightName + "_flicker_node");
    flickerNode->attachObject(light);
}

void RenderManager::rrDestroyMapLight(MapLight* curMapLight)
{
    std::string mapLightName = curMapLight->getOgreNamePrefix() + curMapLight->getName();
    if (mSceneManager->hasLight(mapLightName))
    {
        Ogre::Light* light = mSceneManager->getLight(mapLightName);
        Ogre::SceneNode* lightNode = mSceneManager->getSceneNode(mapLightName + "_node");
        Ogre::SceneNode* lightFlickerNode = mSceneManager->getSceneNode(mapLightName
                                            + "_flicker_node");
        lightFlickerNode->detachObject(light);
        mLightSceneNode->removeChild(lightNode);
        mSceneManager->destroyLight(light);

        if (mSceneManager->hasEntity(mapLightName))
        {
            Ogre::Entity* mapLightIndicatorEntity = mSceneManager->getEntity(
                mapLightName);
            lightNode->detachObject(mapLightIndicatorEntity);
        }
        mSceneManager->destroySceneNode(lightFlickerNode->getName());
        mSceneManager->destroySceneNode(lightNode->getName());
    }
}

void RenderManager::rrDestroyMapLightVisualIndicator(MapLight* curMapLight)
{
    std::string mapLightName = curMapLight->getOgreNamePrefix() + curMapLight->getName();
    if (mSceneManager->hasLight(mapLightName))
    {
        Ogre::SceneNode* mapLightNode = mSceneManager->getSceneNode(mapLightName + "_node");
        std::string mapLightIndicatorName = MapLight::MAPLIGHT_INDICATOR_PREFIX
                                            + curMapLight->getName();
        if (mSceneManager->hasEntity(mapLightIndicatorName))
        {
            Ogre::Entity* mapLightIndicatorEntity = mSceneManager->getEntity(mapLightIndicatorName);
            mapLightNode->detachObject(mapLightIndicatorEntity);
            mSceneManager->destroyEntity(mapLightIndicatorEntity);
            //NOTE: This line throws an error complaining 'scene node not found' that should not be happening.
            //mSceneManager->destroySceneNode(node->getName());
        }
    }
}

void RenderManager::rrPickUpEntity(GameEntity* curEntity)
{
    // Detach the entity from its scene node
    Ogre::SceneNode* curEntityNode = mSceneManager->getSceneNode(curEntity->getOgreNamePrefix() + curEntity->getName() + "_node");
    if(curEntity->getObjectType() == GameEntity::ObjectType::creature)
    {
        mCreatureSceneNode->removeChild(curEntityNode);
    }
    else if(curEntity->getObjectType() == GameEntity::ObjectType::renderedMovableEntity)
    {
        mRoomSceneNode->removeChild(curEntityNode);
    }

    // Attach the creature to the hand scene node
    mSceneManager->getSceneNode("Hand_node")->addChild(curEntityNode);
    //FIXME we should probably use setscale for this, because of rounding.
    curEntityNode->scale(0.333, 0.333, 0.333);

    // Move the other creatures in the player's hand to make room for the one just picked up.
    int i = 0;
    const std::vector<GameEntity*>& objectsInHand = mGameMap->getLocalPlayer()->getObjectsInHand();
    for (std::vector<GameEntity*>::const_iterator it = objectsInHand.begin(); it != objectsInHand.end(); ++it)
    {
        const GameEntity* tmpEntity = *it;
        Ogre::SceneNode* tmpEntityNode = mSceneManager->getSceneNode(tmpEntity->getOgreNamePrefix() + tmpEntity->getName() + "_node");
        tmpEntityNode->setPosition((Ogre::Real)(i % 6 + 1), (Ogre::Real)(i / (int)6), (Ogre::Real)0.0);
        ++i;
    }
}

void RenderManager::rrDropHand(GameEntity* curEntity)
{
    // Detach the entity from the "hand" scene node
    Ogre::SceneNode* curEntityNode = mSceneManager->getSceneNode(curEntity->getOgreNamePrefix() + curEntity->getName() + "_node");
    mSceneManager->getSceneNode("Hand_node")->removeChild(curEntityNode);

    // Attach the creature from the creature scene node
    if(curEntity->getObjectType() == GameEntity::ObjectType::creature)
    {
        mCreatureSceneNode->addChild(curEntityNode);
    }
    else if(curEntity->getObjectType() == GameEntity::ObjectType::renderedMovableEntity)
    {
        mRoomSceneNode->addChild(curEntityNode);
    }
    curEntityNode->setPosition(curEntity->getPosition());
    curEntityNode->scale(3.0, 3.0, 3.0);

    // Move the other creatures in the player's hand to replace the dropped one
    int i = 0;
    const std::vector<GameEntity*>& objectsInHand = mGameMap->getLocalPlayer()->getObjectsInHand();
    for (std::vector<GameEntity*>::const_iterator it = objectsInHand.begin(); it != objectsInHand.end(); ++it)
    {
        const GameEntity* tmpEntity = *it;
        Ogre::SceneNode* tmpEntityNode = mSceneManager->getSceneNode(tmpEntity->getOgreNamePrefix() + tmpEntity->getName() + "_node");
        tmpEntityNode->setPosition((Ogre::Real)(i % 6 + 1), (Ogre::Real)(i / (int)6), (Ogre::Real)0.0);
        ++i;
    }
}

void RenderManager::rrRotateHand()
{
    // Loop over the creatures in our hand and redraw each of them in their new location.
    int i = 0;
    const std::vector<GameEntity*>& objectsInHand = mGameMap->getLocalPlayer()->getObjectsInHand();
    for (std::vector<GameEntity*>::const_iterator it = objectsInHand.begin(); it != objectsInHand.end(); ++it)
    {
        const GameEntity* tmpEntity = *it;
        Ogre::SceneNode* tmpEntityNode = mSceneManager->getSceneNode(tmpEntity->getOgreNamePrefix() + tmpEntity->getName() + "_node");
        tmpEntityNode->setPosition((Ogre::Real)(i % 6 + 1), (Ogre::Real)(i / (int)6), (Ogre::Real)0.0);
        ++i;
    }
}

void RenderManager::rrCreateCreatureVisualDebug(Creature* curCreature, Tile* curTile)
{
    if (curTile != NULL && curCreature != NULL)
    {
        std::stringstream tempSS;
        tempSS << "Vision_indicator_" << curCreature->getName() << "_"
        << curTile->x << "_" << curTile->y;

        Ogre::Entity* visIndicatorEntity = mSceneManager->createEntity(tempSS.str(),
                                           "Cre_vision_indicator.mesh");
        Ogre::SceneNode* visIndicatorNode = mCreatureSceneNode->createChildSceneNode(tempSS.str()
                                            + "_node");
        visIndicatorNode->attachObject(visIndicatorEntity);
        visIndicatorNode->setPosition(Ogre::Vector3((Ogre::Real)curTile->x, (Ogre::Real)curTile->y, (Ogre::Real)0));
        visIndicatorNode->setScale(Ogre::Vector3(BLENDER_UNITS_PER_OGRE_UNIT,
                                   BLENDER_UNITS_PER_OGRE_UNIT,
                                   BLENDER_UNITS_PER_OGRE_UNIT));
    }
}

void RenderManager::rrDestroyCreatureVisualDebug(Creature* curCreature, Tile* curTile)
{
    std::stringstream tempSS;
    tempSS << "Vision_indicator_" << curCreature->getName() << "_"
    << curTile->x << "_" << curTile->y;
    if (mSceneManager->hasEntity(tempSS.str()))
    {
        Ogre::Entity* visIndicatorEntity = mSceneManager->getEntity(tempSS.str());
        Ogre::SceneNode* visIndicatorNode = mSceneManager->getSceneNode(tempSS.str() + "_node");

        visIndicatorNode->detachAllObjects();
        mSceneManager->destroyEntity(visIndicatorEntity);
        mSceneManager->destroySceneNode(visIndicatorNode);
    }
}

void RenderManager::rrSetObjectAnimationState(MovableGameEntity* curAnimatedObject, const std::string& animation, bool loop)
{
    Ogre::Entity* objectEntity = mSceneManager->getEntity(
                                     curAnimatedObject->getOgreNamePrefix()
                                     + curAnimatedObject->getName());

    // Can't animate entities without skeleton
    if (!objectEntity->hasSkeleton())
        return;

    std::string anim = animation;

    // Handle the case where this entity does not have the requested animation.
    if (!objectEntity->getSkeleton()->hasAnimation(anim))
    {
        // Try to change the unexisting animation to a close existing one.
        if (anim == "Flee")
            anim = "Walk";
        else
            anim = "Idle";
    }

    if (objectEntity->getSkeleton()->hasAnimation(anim))
    {
        // Disable the animation for all of the animations on this entity.
        Ogre::AnimationStateIterator animationStateIterator(
            objectEntity->getAllAnimationStates()->getAnimationStateIterator());
        while (animationStateIterator.hasMoreElements())
        {
            animationStateIterator.getNext()->setEnabled(false);
        }

        // Enable the animation specified in the RenderRequest object.
        // FIXME:, make a function rather than using a public var
        curAnimatedObject->mAnimationState = objectEntity->getAnimationState(anim);
        curAnimatedObject->mAnimationState->setTimePosition(0);
        curAnimatedObject->mAnimationState->setLoop(loop);
        curAnimatedObject->mAnimationState->setEnabled(true);
    }
}
void RenderManager::rrMoveSceneNode(const std::string& sceneNodeName, const Ogre::Vector3& position)
{
    if (mSceneManager->hasSceneNode(sceneNodeName))
    {
        Ogre::SceneNode* node = mSceneManager->getSceneNode(sceneNodeName);
        node->setPosition(position);
    }
}

std::string RenderManager::consoleListAnimationsForMesh(const std::string& meshName)
{
    if(!Ogre::ResourceGroupManager::getSingleton().resourceExistsInAnyGroup(meshName + ".mesh"))
        return "\nmesh not available for " + meshName;

    std::string name = meshName + "consoleListAnimationsForMesh";
    Ogre::Entity* objectEntity = msSingleton->mSceneManager->createEntity(name, meshName + ".mesh");
    if (!objectEntity->hasSkeleton())
        return "\nNo skeleton for " + meshName;

    std::string ret;
    Ogre::AnimationStateIterator animationStateIterator(
            objectEntity->getAllAnimationStates()->getAnimationStateIterator());
    while (animationStateIterator.hasMoreElements())
    {
        std::string animName = animationStateIterator.getNext()->getAnimationName();
        ret += "\nAnimation: " + animName;
    }

    Ogre::Skeleton::BoneIterator boneIterator = objectEntity->getSkeleton()->getBoneIterator();
    while (boneIterator.hasMoreElements())
    {
        std::string boneName = boneIterator.getNext()->getName();
        ret += "\nBone: " + boneName;
    }
    msSingleton->mSceneManager->destroyEntity(objectEntity);
    return ret;
}

bool RenderManager::generateRTSSShadersForMaterial(const std::string& materialName,
                                                   const std::string& normalMapTextureName,
                                                   Ogre::RTShader::NormalMapLighting::NormalMapSpace nmSpace)
{
    std::cout << "RenderManager::generateRTSSShadersForMaterial(" << materialName << "," << normalMapTextureName << "," << nmSpace << ")" << std::endl;

    bool success = mShaderGenerator->createShaderBasedTechnique(materialName, Ogre::MaterialManager::DEFAULT_SCHEME_NAME,
                   Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);

    if (!success)
    {
        LogManager::getSingletonPtr()->logMessage("Failed to create shader based technique for: " + materialName
                , Ogre::LML_NORMAL);
        return false;
    }

    Ogre::MaterialPtr material = Ogre::MaterialManager::getSingleton().getByName(materialName);
    //Ogre::Pass* pass = material->getTechnique(0)->getPass(0);
    LogManager::getSingleton().logMessage("Technique and scheme - " + material->getTechnique(0)->getName() + " - "
                                          + material->getTechnique(0)->getSchemeName());
    LogManager::getSingleton().logMessage("Viewport scheme: - " + mViewport->getMaterialScheme());

    Ogre::RTShader::RenderState* renderState = mShaderGenerator->getRenderState(
                Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME, materialName, 0);

    renderState->reset();

    if (normalMapTextureName.empty())
    {
        //per-pixel lighting
        Ogre::RTShader::SubRenderState* perPixelSRS =
            mShaderGenerator->createSubRenderState(Ogre::RTShader::PerPixelLighting::Type);

        renderState->addTemplateSubRenderState(perPixelSRS);
    }
    else
    {
        Ogre::RTShader::SubRenderState* subRenderState = mShaderGenerator->createSubRenderState(
                    Ogre::RTShader::NormalMapLighting::Type);
        Ogre::RTShader::NormalMapLighting* normalMapSRS =
            static_cast<Ogre::RTShader::NormalMapLighting*>(subRenderState);
        normalMapSRS->setNormalMapSpace(nmSpace);
        normalMapSRS->setNormalMapTextureName(normalMapTextureName);

        renderState->addTemplateSubRenderState(normalMapSRS);
    }

    mShaderGenerator->invalidateMaterial(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME, materialName);
    LogManager::getSingletonPtr()->logMessage("Created shader based technique for: " + materialName, Ogre::LML_NORMAL);
    return true;
}

void RenderManager::rtssTest()
{
    generateRTSSShadersForMaterial("Claimed", "Claimed6Nor.png");
    generateRTSSShadersForMaterial("Claimedwall", "Claimedwall2_nor3.png");
    //generateRTSSShadersForMaterial("Dirt", "Dirt_dark_nor3.png");
    //generateRTSSShadersForMaterial("Dormitory", "Dirt_dark_nor3.png");
    //TODO - fix this model so it doesn't use the material name 'material'
    generateRTSSShadersForMaterial("Material", "Forge_normalmap.png");
    generateRTSSShadersForMaterial("Troll2", "Troll2_nor2.png");
    generateRTSSShadersForMaterial("Kobold_skin/TEXFACE/kobold_skin6.png");
    generateRTSSShadersForMaterial("Kobold_skin/TWOSIDE/TEXFACE/kobold_skin6.png");
    generateRTSSShadersForMaterial("Wizard/TWOSIDE", "Wizard_nor.png");
    generateRTSSShadersForMaterial("Wizard", "Wizard_nor.png");
    generateRTSSShadersForMaterial("Kreatur", "Kreatur_nor2.png");
    generateRTSSShadersForMaterial("Wyvern", "Wyvern_red_normalmap.png");
    //generateRTSSShadersForMaterial("Gold", "Dirt_dark_nor3.png");
    generateRTSSShadersForMaterial("Roundshield");
    generateRTSSShadersForMaterial("Staff");

    mShaderGenerator->invalidateScheme(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
}

Ogre::Entity* RenderManager::createEntity(const std::string& entityName, const std::string& meshName,
                                          const std::string& normalMapTextureName)
{
    std::cout << "RenderManager::createEntity(" << entityName << "," << meshName << "," << normalMapTextureName << ")" << std::endl;
    //TODO - has to be changed a bit, shaders shouldn't be generated here.
    Ogre::Entity* ent = mSceneManager->createEntity(entityName, meshName);

    Ogre::MeshPtr meshPtr = ent->getMesh();
    unsigned short src, dest;
    if (!meshPtr->suggestTangentVectorBuildParams(Ogre::VES_TANGENT, src, dest))
    {
        meshPtr->buildTangentVectors(Ogre::VES_TANGENT, src, dest);
    }
    //Generate rtss shaders
    Ogre::Mesh::SubMeshIterator it = meshPtr->getSubMeshIterator();
    while (it.hasMoreElements())
    {
        Ogre::SubMesh* subMesh = it.getNext();
        LogManager::getSingleton().logMessage("Trying to generate shaders for material: " + subMesh->getMaterialName());
        generateRTSSShadersForMaterial(subMesh->getMaterialName(), normalMapTextureName);
    }
    return ent;
}

void RenderManager::colourizeEntity(Ogre::Entity *ent, Seat* seat, bool markedForDigging)
{
    //Disabled for normal mapping. This has to be implemented in some other way.

    // Colorize the the textures
    // Loop over the sub entities in the mesh
    if (seat == NULL && !markedForDigging)
        return;

    for (unsigned int i = 0; i < ent->getNumSubEntities(); ++i)
    {
        Ogre::SubEntity *tempSubEntity = ent->getSubEntity(i);
        tempSubEntity->setMaterialName(colourizeMaterial(tempSubEntity->getMaterialName(), seat, markedForDigging));
    }
}

std::string RenderManager::colourizeMaterial(const std::string& materialName, Seat* seat, bool markedForDigging)
{
    std::stringstream tempSS;
    Ogre::Technique *tempTechnique;
    Ogre::Pass *tempPass;

    tempSS.str("");

    // Create the material name.
    if(seat != nullptr)
        tempSS << "Color_" << seat->getColorId() << "_" ;
    else
        tempSS << "Color_0_" ;

    if (markedForDigging)
        tempSS << "dig_";

    tempSS << materialName;
    Ogre::MaterialPtr newMaterial = Ogre::MaterialPtr(Ogre::MaterialManager::getSingleton().getByName(tempSS.str()));

    //cout << "\nCloning material:  " << tempSS.str();

    // If this texture has been copied and colourized, we can return
    if (!newMaterial.isNull())
        return tempSS.str();

    // If not yet, then do so

    // Check to see if we find a seat with the requested color, if not then just use the original, uncolored material.
    if (seat == NULL && markedForDigging == false)
        return materialName;

    //std::cout << "\nMaterial does not exist, creating a new one.";
    newMaterial = Ogre::MaterialPtr(
                        Ogre::MaterialManager::getSingleton().getByName(materialName))->clone(tempSS.str());

    // Loop over the techniques for the new material
    for (unsigned int j = 0; j < newMaterial->getNumTechniques(); ++j)
    {
        tempTechnique = newMaterial->getTechnique(j);
        if (tempTechnique->getNumPasses() == 0)
            continue;

        if (markedForDigging)
        {
            // Color the material with yellow on the latest pass
            // so we're sure to see the taint.
            tempPass = tempTechnique->getPass(tempTechnique->getNumPasses() - 1);
            Ogre::ColourValue color(1.0, 1.0, 0.0, 0.3);
            tempPass->setEmissive(color);
            tempPass->setSpecular(color);
            tempPass->setAmbient(color);
            tempPass->setDiffuse(color);
        }
        else if (seat != nullptr)
        {
            // Color the material with the Seat's color.
            tempPass = tempTechnique->getPass(0);
            Ogre::ColourValue color = seat->getColorValue();
            color.a = 0.3;
            tempPass->setEmissive(color);
            tempPass->setAmbient(color);
            // Remove the diffuse light to avoid the fluorescent effect.
            tempPass->setDiffuse(Ogre::ColourValue(0.0, 0.0, 0.0));
        }
    }

    return tempSS.str();

}
