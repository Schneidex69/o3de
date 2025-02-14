/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include "EMotionFXConfig.h"
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/typetraits/integral_constant.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Color.h>

// include MCore related files
#include <MCore/Source/AABB.h>
#include <MCore/Source/Vector.h>
#include <MCore/Source/Array.h>
#include <MCore/Source/SmallArray.h>
#include <MCore/Source/OBB.h>
#include <MCore/Source/Distance.h>

// include required headers
#include <EMotionFX/Source/PhysicsSetup.h>
#include "BaseObject.h"
#include "Pose.h"
#include "Skeleton.h"

#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Model/MorphTargetMetaAsset.h>
#include <Atom/RPI.Reflect/Model/SkinMetaAsset.h>

namespace EMotionFX
{
    // forward declarations.
    class ActorInstance;
    class NodeMap;
    class AnimGraph;
    class Node;
    class Material;
    class MorphSetup;
    class NodeGroup;
    class SimulatedObjectSetup;
    class Skeleton;
    class Mesh;
    class MeshDeformerStack;

    /**
     * The actor is the representation of a completely animatable object, like a human character or an animal.
     * It represents a (read only) shared data object, from which ActorInstance objects can be created.
     * The actor instance objects are used as characters in the game and can all be controlled individually, while they
     * still share the same data from the Actor class. The Actor contains information about the hierarchy/structure of the characters.
     */
    class EMFX_API Actor
    {
    public:
        AZ_CLASS_ALLOCATOR_DECL
        AZ_RTTI(Actor, "{15F0DAD5-6077-45E8-A628-1DB8FAFFE1BE}")

        /**
         * An actor dependency, which can be used during multithread scheduling.
         */
        struct EMFX_API Dependency
        {
            Actor*      mActor;        /**< The actor where the instance is dependent on. */
            AnimGraph*  mAnimGraph;    /**< The anim graph we depend on. */
        };

        //
        enum
        {
            MIRRORFLAG_INVERT_X = 1 << 0,   // NOTE: do not combine the MIRRORFLAG_INVERT_X with INVERT_Y or INVERT_Z
            MIRRORFLAG_INVERT_Y = 1 << 1,
            MIRRORFLAG_INVERT_Z = 1 << 2
        };

        enum EAxis : uint8
        {
            AXIS_X = 0,
            AXIS_Y = 1,
            AXIS_Z = 2
        };

        // per node mirror info
        struct EMFX_API NodeMirrorInfo
        {
            uint16  mSourceNode;        // from which node to extract the motion
            uint8   mAxis;              // X=0, Y=1, Z=2
            uint8   mFlags;             // bitfield with MIRRORFLAG_ prefix
        };

        enum class LoadRequirement : bool
        {
            RequireBlockingLoad,
            AllowAsyncLoad
        };

        //------------------------------------------------

        /**
        * @param name The name of the actor.
        */
        explicit Actor(const char* name);

        virtual ~Actor();

        /**
         * Get the unique identification number for the actor.
         * @return The unique identification number.
         */
        MCORE_INLINE uint32 GetID() const                                       { return mID; }

        /**
         * Set the unique identification number for the actor instance.
         * @param[in] id The unique identification number.
         */
        MCORE_INLINE void SetID(uint32 id)                                      { mID = id; }

        /**
         * Add a node to this actor.
         * @param node The node to add.
         */
        void AddNode(Node* node);

        /**
         * Add a node to this actor.
         */
        Node* AddNode(uint32 nodeIndex, const char* name, uint32 parentIndex = MCORE_INVALIDINDEX32);

        /**
         * Remove a given node.
         * @param nr The node to remove.
         * @param delMem If true the allocated memory of the node will be deleted.
         */
        void RemoveNode(uint32 nr, bool delMem = true);

        /**
         * Remove all nodes from memory.
         */
        void DeleteAllNodes();

        /**
         * Clones this actor.
         * @return A pointer to the duplicated clone.
         */
        AZStd::unique_ptr<Actor> Clone() const;

        /**
         * Scale all transform and mesh positional data.
         * This is a very slow operation and is used to convert between different unit systems (cm, meters, etc).
         * @param scaleFactor The scale factor to scale the current data by.
         */
        void Scale(float scaleFactor);

        /**
         * Scale to a given unit type.
         * This method does nothing if the actor is already in this unit type.
         * You can check what the current unit type is with the GetUnitType() method.
         * @param targetUnitType The unit type to scale into (meters, centimeters, etc).
         */
        void ScaleToUnitType(MCore::Distance::EUnitType targetUnitType);

        //---------------------------------------------------------------------

        /**
         * Try to find the best motion extraction node automatically.
         * It picks the node with the most number of child nodes down the hierarchy below that node.
         * @result A pointer to the best suitable motion extraction node, or nullptr when no node could be found.
         */
        Node* FindBestMotionExtractionNode() const;

        /**
         * Automatically find the best motion extraction node, using FindBestMotionExtractionNode and set it as current
         * motion extraction node.
         */
        void AutoSetMotionExtractionNode();

        /**
         * Generate an update path from a given node towards the root.
         * The first element in the array will be the end node index, the next node will be its parent node, etc.
         * So the last node will be the root node.
         * @param endNodeIndex The node index to generate the path to.
         * @param outPath the array that will contain the path.
         */
        void GenerateUpdatePathToRoot(uint32 endNodeIndex, MCore::Array<uint32>& outPath) const;

        /**
         * Set the motion extraction node.
         * This is the node from which we filter the motion. Most likely the hips node.
         * The filtered motion of this node is applied to the actor instance.
         * You can set the node to nullptr in case you want to disable motion extraction.
         * @param node The motion extraction node, or nullptr to disable it.
         */
        void SetMotionExtractionNode(Node* node);

        /**
         * Set the motion extraction node index.
         * This is the node from which we filter the motion. Most likely the hips node.
         * The filtered motion of this node is applied to the actor instance.
         * You can set the node to MCORE_INVALIDINDEX32 in case you want to disable motion extraction.
         * @param nodeIndex The motion extraction node, or MCORE_INVALIDINDEX32 to disable it.
         */
        void SetMotionExtractionNodeIndex(uint32 nodeIndex);

        /**
         * Get the motion extraction node.
         * @result The motion extraction node, or nullptr when it has not been set.
         */
        Node* GetMotionExtractionNode() const;

        /**
         * Get the motion extraction node index.
         * @result The motion extraction node index, or MCORE_INVALIDINDEX32 when it has not been set.
         */
        MCORE_INLINE uint32 GetMotionExtractionNodeIndex() const                        { return mMotionExtractionNode; }

        //---------------------------------------------------------------------

        /**
         * Check if this actor contains any nodes that have meshes.
         * @param lodLevel The LOD level to check for.
         * @result Returns true when this actor contains nodes that have meshes in the given LOD, otherwise false is returned.
         */
        bool CheckIfHasMeshes(uint32 lodLevel) const;

        /**
         * Check if we have skinned meshes.
         * @param lodLevel The LOD level to check for.
         * @result Returns true when skinned meshes are present in the specified LOD level, otherwise false is returned.
         */
        bool CheckIfHasSkinnedMeshes(AZ::u32 lodLevel) const;

        /**
         * Extract a list with nodes that represent bones.
         * Not all nodes inside an actor have to be bones. With bones we mean nodes that appear inside the skinning
         * information of the meshes. So nodes that have vertices linked to them.
         * Extracting this information is not fast, so shouldn't be done inside a loop.
         * @param lodLevel The LOD level, which must be in range of 0..GetNumLODLevels().
         * @param outBoneList The array of indices to nodes that will be filled with the nodes that are bones. When the outBoneList array
         *                    already contains items, the array will first be cleared, so all existing contents will be lost.
         */
        void ExtractBoneList(uint32 lodLevel, MCore::Array<uint32>* outBoneList) const;

        //------------------------------------------------
        void SetPhysicsSetup(const AZStd::shared_ptr<PhysicsSetup>& physicsSetup);
        const AZStd::shared_ptr<PhysicsSetup>& GetPhysicsSetup() const;

        //------------------------------------------------
        void SetSimulatedObjectSetup(const AZStd::shared_ptr<SimulatedObjectSetup>& setup);
        const AZStd::shared_ptr<SimulatedObjectSetup>& GetSimulatedObjectSetup() const;

        /**
         * Pre-allocate space to store a given amount of materials.
         * This does not have any effect on the value returned by GetNumMaterials().
         * @param lodLevel The geometry LOD level to work on.
         * @param numMaterials The amount of materials to pre-allocate space for.
         */
        void ReserveMaterials(uint32 lodLevel, uint32 numMaterials);

        /**
         * Get a given material.
         * @param lodLevel The LOD level to get the material from.
         * @param nr The material number to get.
         * @result A pointer to the material.
         */
        Material* GetMaterial(uint32 lodLevel, uint32 nr) const;

        /**
         * Find the material number/index of the material with the specified name.
         * Please note that this check is case sensitive.
         * @param lodLevel The level of detail to get the material from.
         * @param name The name of the material.
         * @result Returns the material number/index, which you can use to GetMaterial. When no material with the given name
         *         can be found, a value of MCORE_INVALIDINDEX32 is returned.
         */
        uint32 FindMaterialIndexByName(uint32 lodLevel, const char* name) const;

        /**
         * Set a given material.
         * @param lodLevel The LOD level to set the material at.
         * @param nr The material number to set.
         * @param mat The material to set at this index.
         */
        void SetMaterial(uint32 lodLevel, uint32 nr, Material* mat);

        /**
         * Add a material to the back of the material list.
         * @param lodLevel The LOD level add the material to.
         * @param mat The material to add to the back of the list.
         */
        void AddMaterial(uint32 lodLevel, Material* mat);

        /**
         * Remove the given material from the material list and reassign all material numbers of the sub meshes
         * since the material order will be changed after removing a material. This means that several sub meshes
         * do not point to their correct materials anymore or that they would be even out of range. If one of the sub
         * meshes got a bigger material index number than the material which has been removed, the offset of the node
         * to which this sub mesh belongs to will be changed so that the sub mesh point to its right material again.
         * This will be fixed by decreasing their material offset.
         * @param lodLevel The LOD level add the material to.
         * @param index The material index of the material to remove.
         */
        void RemoveMaterial(uint32 lodLevel, uint32 index);

        /**
         * Get the number of materials.
         * @param lodLevel The LOD level to get the number of material from.
         * @result The number of materials this actor has/uses.
         */
        uint32 GetNumMaterials(uint32 lodLevel) const;

        /**
         * Removes all materials from this actor.
         */
        void RemoveAllMaterials();

        /**
         * Check whether the given material is used by one of the meshes in the actor.
         * So checks each material with the the material which is passed as parameter if they point to the same material
         * object in memory. If they are equal it returns true, this means that the given material is used by a mesh of
         * the actor. False means that no mesh uses the given material.
         * @param lodLevel The LOD level of the material to check.
         * @param index The material number to check.
         * @result Returns true when there are meshes using the material, otherwise false is returned.
         */
        bool CheckIfIsMaterialUsed(uint32 lodLevel, uint32 index) const;

        //------------------------------------------------

        /**
         * Add a LOD level.
         * @param[in] copyFromLastLevelLOD True in case the new LOD level should contain the same meshes as the last LOD level. In case of false the meshes and everything won't be copied over.
         */
        void AddLODLevel(bool copyFromLastLODLevel = true);

        /**
         * Copy data from a given LOD level to another one.
         * This will copy the skeletal LOD flag for the nodes, copy over the meshes and their deformer stacks, the materials as well as the whole morph setup.
         * @param[in] copyActor The actor to copy the LOD level from.
         * @param[in] copyLODLevel The LOD level from which we get the data from.
         * @param[in] replaceLODLevel The LOD level to which we copy the data to.
         * @param[in] copySkeletalLODFlags Copy over the skeletal LOD flags in case of true, skip them in case of false.
         * @param[in] delLODActorFromMem When set to true, the method will automatically delete the given copyActor from memory.
         */
        void CopyLODLevel(Actor* copyActor, uint32 copyLODLevel, uint32 replaceLODLevel, bool copySkeletalLODFlags);

        /**
         * Insert LOD level at the given position.
         * This function will not copy any meshes, deformer, morph targets or materials but just insert an empty LOD level.
         * @param[in] insertAt The position to insert the new LOD level.
         */
        void InsertLODLevel(uint32 insertAt);

        /**
         * Set the number of LOD levels.
         * This will be called by the importer. Do not use manually.
         */
        void SetNumLODLevels(uint32 numLODs, bool adjustMorphSetup = true);

        /**
         * Get the number of LOD levels inside this actor.
         * @result The number of LOD levels. This value is at least 1, since the full detail LOD is always there.
         */
        uint32 GetNumLODLevels() const;

        //--------------------------------------------------------------------------

        /**
         * Removes all meshes from all nodes inside this actor.
         * This means all memory will be released and pointers to the meshes will not be valid anymore.
         * Also all mesh deformer stacks will be removed.
         */
        void RemoveAllNodeMeshes();

        /**
         * Calculates the total number of vertices and indices of all node meshes for the given LOD.
         * @param lodLevel The LOD level, where 0 is the highest detail LOD level. This value must be in range of [0..GetNumLODLevels()-1].
         * @param outNumPolygons The integer to write the number of polygons in.
         * @param outNumVertices The integer to write the number of vertices in.
         * @param outNumIndices The integer to write the number of indices in.
         */
        void CalcMeshTotals(uint32 lodLevel, uint32* outNumPolygons, uint32* outNumVertices, uint32* outNumIndices) const;

        /**
         * Calculates the total number of vertices and indices of all STATIC node meshes for the given LOD.
         * With static we mean the meshes that are NOT effected by any deformers, so which are completely rigid.
         * @param lodLevel The LOD level, where 0 is the highest detail LOD level. This value must be in range of [0..GetNumLODLevels()-1].
         * @param outNumVertices The integer to write the number of vertices in.
         * @param outNumIndices The integer to write the number of indices in.
         */
        void CalcStaticMeshTotals(uint32 lodLevel, uint32* outNumVertices, uint32* outNumIndices);

        /**
         * Calculates the total number of vertices and indices of all DEFORMABLE node meshes for the given LOD.
         * With deformable we mean meshes that are being modified by mesh deformers, such as skinning or morphing deformers.
         * The number of faces can be calculated by dividing the resulting number of indices by 3.
         * @param lodLevel The LOD level, where 0 is the highest detail LOD level. This value must be in range of [0..GetNumLODLevels()-1].
         * @param outNumVertices The integer to write the number of vertices in.
         * @param outNumIndices The integer to write the number of indices in.
         */
        void CalcDeformableMeshTotals(uint32 lodLevel, uint32* outNumVertices, uint32* outNumIndices);

        /**
         * Calculates the maximum number of bone influences.
         * This is calculated by for each vertex checking the number of bone influences, and take the maximum of that amount.
         * @param lodLevel The LOD level, where 0 is the highest detail LOD level. This value must be in range of [0..GetNumLODLevels()-1].
         * @result The maximum number of influences. This will be 0 for non-softskinned objects.
         */
        uint32 CalcMaxNumInfluences(uint32 lodLevel) const;

        /**
         * Calculates the maximum number of bone influences.
         * Also provides an array containing the number of vertices for each number of influences.
         * @param lodLevel The LOD level, where 0 is the highest detail LOD level. This value must be in range of [0..GetNumLODLevels()-1].
         * @param vertexCounts An output array, which will contain for each number of influences, the number of vertices with this number of
         * influences. In other words, the first element of the array will contain the number of vertices, which have 0 influences.
         * The next element in the array will contain the number of vertices, which are influenced by exactly 1 bone, etc.
         * @param lodLevel The detail level to calculate the results for. A value of 0 is the highest detail.
         * @result The maximum number of vertex/bone influences. This will be 0 for rigid, non-skinned objects.
         */
        uint32 CalcMaxNumInfluences(uint32 lodLevel, AZStd::vector<uint32>& outVertexCounts) const;

        /**
         * Verify if the skinning will look correctly in the given geometry LOD for a given skeletal LOD level.
         * As the skeletal LOD system can disable nodes entirely the skinning info of a mesh might be linked to a
         * disabled node. This will end up in in an incorrectly deformed mesh.
         * @param conflictNodeFlags The array of flags that indicate whether a node is used by the skinning info of the geometry LOD level
         *        but is disabled by the given skeletal LOD level. Nodes which have an enabled state after calling the function
         *        will be nodes that are responsible for bad skinning. The size of the array will always be equal to the number of nodes.
         *        So if the conflictNodeFlags[myNodeNr] equals 1 you know there is a conflict, while everything is alright if the value is 0.
         * @param skeletalLODLevel The skeletal LOD level to be verified. The skinning influences will be tested against
         *                         disabled nodes from the given skeletal LOD level.
         * @param geometryLODLevel The geometry LOD level to test the skeletal LOD against with.
         */
        void VerifySkinning(MCore::Array<uint8>& conflictNodeFlags, uint32 skeletalLODLevel, uint32 geometryLODLevel);

        /**
         * Checks if the given material is used by a given mesh.
         * @param mesh The mesh to check.
         * @param materialIndex The index of the material to check.
         * @return True if one of the submeshes of the given mesh uses the given material, false if not.
         */
        bool CheckIfIsMaterialUsed(Mesh* mesh, uint32 materialIndex) const;

        //------------------

        /**
         * Get a pointer to the custom data you stored.
         * Custom data can for example link a game or engine object with this EMotion FX actor object.
         * An example is when EMotion FX triggers a motion event. You know the actor that triggered the event, but
         * you don't know directly what game or engine object is linked to this actor.
         * By using the custom data methods GetCustomData and SetCustomData you can set a pointer to your game or engine
         * object in each actor.
         * The pointer that you specify will not be deleted when the actor object is being destructed.
         * @result A void pointer to the custom data you have specified.
         */
        void* GetCustomData() const;

        /**
         * Get a pointer to the custom data you stored.
         * Custom data can for example link a game or engine object with this EMotion FX actor object.
         * An example is when EMotion FX triggers a motion event. You know the actor that triggered the event, but
         * you don't know directly what game or engine object is linked to this actor.
         * By using the custom data methods GetCustomData and SetCustomData you can set a pointer to your game or engine
         * object in each actor.
         * The pointer that you specify will not be deleted when the actor object is being destructed.
         * @param dataPointer A void pointer to the custom data, which could for example be your engine or game object that is linked to this actor.
         */
        void SetCustomData(void* dataPointer);

        /**
         * Set the name of the actor.
         * @param name The name of the actor.
         */
        void SetName(const char* name);

        /**
         * Get the name of the actor.
         * @result The name of the actor.
         */
        const char* GetName() const;

        /**
         * Get the name of the actor as a Core string object.
         * @result The string containing the name of the actor.
         */
        const AZStd::string& GetNameString() const;

        /**
         * Set the filename of the actor.
         * @param[in] filename The filename of the actor.
         */
        void SetFileName(const char* filename);

        /**
         * Get the filename of the actor.
         * @result The filename of the actor.
         */
        const char* GetFileName() const;

        /**
         * Returns the filename of the actor, as a AZStd::string object.
         * @result The filename of the actor.
         */
        const AZStd::string& GetFileNameString() const;

        /**
         * Add a dependency to the actor.
         * Dependencies are used to identify on what other actor objects this actor relies.
         * This can be because this actor uses meshes or transforms that are stored inside the other actor.
         * @param dependency The dependency to add.
         */
        void AddDependency(const Dependency& dependency);

        /**
         * Get the number of dependencies.
         * @result The number of dependencies that this actor has on other actors.
         */
        MCORE_INLINE uint32 GetNumDependencies() const                          { return mDependencies.GetLength(); }

        /**
         * Get a given dependency.
         * @param nr The dependency number, which must be in range of [0..GetNumDependencies()-1].
         * @result A pointer to the dependency.
         */
        MCORE_INLINE Dependency* GetDependency(uint32 nr)                       { return &mDependencies[nr]; }
        MCORE_INLINE const Dependency* GetDependency(uint32 nr) const           { return &mDependencies[nr]; }

        /**
         * Recursively add dependencies that this actor has on other actors.
         * This adds the dependencies of the actors on which we currently have dependencies, and that recursively.
         * So after executing this, the current actor contains all dependencies of all the actors it is dependent on.
         * @param actor The actor to create the dependencies for.
         */
        void RecursiveAddDependencies(const Actor* actor);

        /**
         * Get the morph setup at a given geometry LOD level.
         * @param geomLODLevel The geometry LOD level, which must be in range of [0..GetNumLODLevels()].
         * @result A smart pointer object to the morph setup. Use the MCore::Pointer<MorphSetup>::GetPointer() to get the actual pointer.
         *         That GetPointer() method will return nullptr when there is no morph setup for the given LOD level.
         */
        MCORE_INLINE MorphSetup* GetMorphSetup(uint32 geomLODLevel) const       { return mMorphSetups[geomLODLevel]; }

        /**
         * Remove all morph setups. Morph setups contain all morph targtets.
         * @param deleteMeshDeformers When set to true (default), mesh deformers, such as the morphing deformer, will be deleted from all nodes.
         */
        void RemoveAllMorphSetups(bool deleteMeshDeformers = true);

        /**
         * Set the  morph setup for a given geometry LOD level.
         * It is possible to set the setup for a given LOD level to nullptr.
         * If the setup is nullptr, no  morphing is being processed for the given LOD.
         * @param lodLevel The LOD level, which must be in range of [0..GetNumLODLevels()-1].
         * @param setup The  morph setup for this LOD.
         */
        void SetMorphSetup(uint32 lodLevel, MorphSetup* setup);

        /**
         * Update the oriented bounding volumes (OBB) of all the nodes inside this actor.
         * This is a very heavy calculation and must NOT be performed on a per-frame basis but only as pre-process step.
         * The OBBs of the nodes are already being calculated at export time, so you shouldn't really need to use this method.
         * Only when the bind pose geometry has changed you can update the node OBBs by calling this method.
         * For more information about how the bounds are calculated please see the Node::GetOBB() and Node::CalcOBBFromBindPose() methods.
         * The calculations performed by this method are automatically spread over multiple threads to improve the performance.
         * @param lodLevel The geometry LOD level to use while calculating the object oriented bounds per node.
         */
        void UpdateNodeBindPoseOBBs(uint32 lodLevel);

        /**
         * Get the number of node groups inside this actor object.
         * @result The number of node groups.
         */
        uint32 GetNumNodeGroups() const;

        /**
         * Get a pointer to a given node group.
         * @param index The node group index, which must be in range of [0..GetNumNodeGroups()-1].
         */
        NodeGroup* GetNodeGroup(uint32 index) const;

        /**
         * Add a node group.
         * @param newGroup The new node group to add.
         */
        void AddNodeGroup(NodeGroup* newGroup);

        /**
         * Remove a given node group by its index.
         * @param index The node group number to remove. This value must be in range of [0..GetNumNodeGroups()-1].
         * @param delFromMem Set to true (default) when you wish to also delete the specified group from memory.
         */
        void RemoveNodeGroup(uint32 index, bool delFromMem = true);

        /**
         * Remove a given node group by its pointer.
         * @param group The node group to remove. Please keep in mind that this group should really be part of this actor.
         * @param delFromMem Set to true (default) when you wish to also delete the specified group from memory.
         *                   Even if this group is not part of this actor and delFromMem is set to true, the group will be deleted from memory.
         */
        void RemoveNodeGroup(NodeGroup* group, bool delFromMem = true);

        /**
         * Find a node group index by its name.
         * @param groupName The name of the group to search for. This is case sensitive.
         * @result The group number, or MCORE_INVALIDINDEX32 when it cannot be found.
         */
        uint32 FindNodeGroupIndexByName(const char* groupName) const;

        /**
         * Find a group index by its name, on a non-case sensitive way.
         * @param groupName The name of the group to search for. This is NOT case sensitive.
         * @result The group number, or MCORE_INVALIDINDEX32 when it cannot be found.
         */
        uint32 FindNodeGroupIndexByNameNoCase(const char* groupName) const;

        /**
         * Find a node group by its name.
         * @param groupName The name of the group to search for. This is case sensitive.
         * @result A pointer to the node group, or nullptr when it cannot be found.
         */
        NodeGroup* FindNodeGroupByName(const char* groupName) const;

        /**
         * Find a node group by its name, but without case sensitivity.
         * @param groupName The name of the group to search for. This is NOT case sensitive.
         * @result A pointer to the node group, or nullptr when it cannot be found.
         */
        NodeGroup* FindNodeGroupByNameNoCase(const char* groupName) const;

        /**
         * Remove all node groups from this actor.
         * This also deletes the groups from memory.
         */
        void RemoveAllNodeGroups();

        //-------------------

        /**
         * Allocate data for the node motion mirror info.
         * This resizes the array of node motion infos and initializes it on default values.
         */
        void AllocateNodeMirrorInfos();

        /**
         * Remove memory for the motion node source array.
         * If the motion node source array has not been allocated before it will do nothing at all.
         */
        void RemoveNodeMirrorInfos();

        /**
         * Get the mirror info for a given node.
         * @param nodeIndex The node index to get the info for.
         * @result A reference to the mirror info.
         */
        MCORE_INLINE NodeMirrorInfo& GetNodeMirrorInfo(uint32 nodeIndex)                            { return mNodeMirrorInfos[nodeIndex]; }

        /**
         * Get the mirror info for a given node.
         * @param nodeIndex The node index to get the info for.
         * @result A reference to the mirror info.
         */
        MCORE_INLINE const NodeMirrorInfo& GetNodeMirrorInfo(uint32 nodeIndex) const                { return mNodeMirrorInfos[nodeIndex]; }

        MCORE_INLINE bool GetHasMirrorInfo() const                                                  { return (mNodeMirrorInfos.GetLength() != 0); }

        //---------------------------------------------------------------

        /**
         * Match and map the motion sources of given nodes, based on two substrings.
         * This is used for motion mirroring, where we want to play the motion of the left arm on the right arm for example.
         * If you have nodes named like "Left Leg", "Left Arm" "My Left Foot" and "My Right Foot", "Right Leg" and "Right Arm", then you can use the following substrings as parameters
         * to match everything automatically: subStringA="Left" and subStringB="Right".
         * Nodes that have no mirrored match (for example the spine bones) will mirror their own transforms when mirroring is enabled.
         * It doesn't matter if you put the "left" or "right" substring inside subStringA or subStringB. The order of them doesn't matter.
         * Also please note that artists can already setup all this, so your model is probably already prepared for mirroring once loaded if you are planning to use motion mirroring.
         * @param subStringA The first substring, which could be something like "Left" or "Bip01 " or " L ".
         * @param subStringB The second substring, which could be something like "Right" or "Bip01 R" or " R ".
         */
        void MatchNodeMotionSources(const char* subStringA, const char* subStringB);

        /**
         * Map two nodes to eachother for motion mirroring.
         * This could be used to tell to play the motion for the right arm on the left arm.
         * The parameters you would give would be like: "Left Arm" and "Right Arm" in that case.
         * @param sourceNodeName The name of the first node, for example "Left Hand".
         * @param destNodeName The name of the second node, for example "Right Hand".
         * @result Returns true when the mapping was successful, or false when one of the nodes cannot be found.
         */
        bool MapNodeMotionSource(const char* sourceNodeName, const char* destNodeName);

        bool MapNodeMotionSource(uint16 sourceNodeIndex, uint16 targetNodeIndex);

        /**
         * Find the best match for a given node, using two substrings.
         * For example, if you use the node name "Right Leg" and you use as subStringA "Right" and as subStringB "Left", then it will
         * result most likely (asssuming that node exists) in the node with the name "Left Leg".
         * You can use this to guess the most most likely match for a given node, when using motion mirroring.
         * Please note that artists can all setup this already. If you want to do it manually you might also want to use the MatchNodeMotionSources function instead.
         * @param nodeName The name of the node to find the counter-mirror-node for.
         * @param subStringA The first substring, for example "Left".
         * @param subStringB The second substring, for example "Right".
         * @param firstPass Basically just set this to true always, which is also its default.
         * @result The node index for the node that matches best, or MCORE_INVALIDINDEX16 (please note the 16 and NOT 32) when no good match has been found.
         */
        uint16 FindBestMatchForNode(const char* nodeName, const char* subStringA, const char* subStringB, bool firstPass = true) const;

        void MatchNodeMotionSourcesGeometrical();
        uint16 FindBestMirrorMatchForNode(uint16 nodeIndex, Pose& pose) const;

        /**
         * Set the dirty flag which indicates whether the user has made changes to the actor or not. This indicator is set to true
         * when the user changed something like adding a new node group. When the user saves the actor, the indicator is usually set to false.
         * @param dirty The dirty flag.
         */
        void SetDirtyFlag(bool dirty);

        /**
         * Get the dirty flag which indicates whether the user has made changes to the actor or not. This indicator is set to true
         * when the user changed something like adding a new node group. When the user saves the actor, the indicator is usually set to false.
         * @return The dirty flag.
         */
        bool GetDirtyFlag() const;

        void SetIsUsedForVisualization(bool flag);
        bool GetIsUsedForVisualization() const;

        /**
         * Marks the actor as used by the engine runtime, as opposed to the tool suite.
         */
        void SetIsOwnedByRuntime(bool isOwnedByRuntime);
        bool GetIsOwnedByRuntime() const;

        /**
         * Recursively find the parent bone that is enabled in a given LOD, starting from a given node.
         * For example if you have a finger bone, while the finger bones are disabled in the skeletal LOD, this function will return the index to the hand bone.
         * This is because the hand bone will most likely be the first active bone in that LOD, when moving up the hierarchy.
         * @param skeletalLOD The skeletal LOD level to search in.
         * @param startNodeIndex The node to start looking at, for example the node index of the finger bone.
         * @result Returns the index of the first active node, when moving up the hierarchy towards the root node. Returns MCORE_INVALIDINDEX32 when not found.
         */
        uint32 FindFirstActiveParentBone(uint32 skeletalLOD, uint32 startNodeIndex) const;

        /**
         * Make the geometry LOD levels compatible with the skinning LOD levels.
         * This means that it modifies the skinning information so that disabled bones are not used by the meshes anymore.
         * Instead, the skinning influences are remapped to the first enabled parent bone. So if you disable all finger bones in a given LOD, it will
         * adjust the skinning influences so that the vertices of the fingers are linked to the hand bone instead.
         */
        void MakeGeomLODsCompatibleWithSkeletalLODs();

        void ReinitializeMeshDeformers();
        void PostCreateInit(bool makeGeomLodsCompatibleWithSkeletalLODs = true, bool generateOBBs = true, bool convertUnitType = true);

        void AutoDetectMirrorAxes();
        const MCore::Array<NodeMirrorInfo>& GetNodeMirrorInfos() const;
        MCore::Array<NodeMirrorInfo>& GetNodeMirrorInfos();
        void SetNodeMirrorInfos(const MCore::Array<NodeMirrorInfo>& mirrorInfos);
        bool GetHasMirrorAxesDetected() const;

        MCORE_INLINE const AZStd::vector<Transform>& GetInverseBindPoseTransforms() const                               { return mInvBindPoseTransforms; }
        MCORE_INLINE Pose* GetBindPose()                                                                                { return mSkeleton->GetBindPose(); }
        MCORE_INLINE const Pose* GetBindPose() const                                                                    { return mSkeleton->GetBindPose(); }

        /**
         * Get the inverse bind pose (in world space) transform of a given joint.
         * @param jointIndex The joint number, which must be in range of [0..GetNumNodes()-1].
         * @result The inverse of the bind pose transform.
         */
        MCORE_INLINE const Transform& GetInverseBindPoseTransform(uint32 nodeIndex) const                         { return mInvBindPoseTransforms[nodeIndex]; }

        void ReleaseTransformData();
        void ResizeTransformData();
        void CopyTransformsFrom(const Actor* other);

        const MCore::AABB& GetStaticAABB() const;
        void SetStaticAABB(const MCore::AABB& box);
        void UpdateStaticAABB();    // VERY heavy operation, you shouldn't call this ever (internally creates an actor instance, updates mesh deformers, calcs a mesh based aabb, destroys the actor instance again)

        void SetThreadIndex(uint32 index)                   { mThreadIndex = index; }
        uint32 GetThreadIndex() const                       { return mThreadIndex; }

        Mesh* GetMesh(uint32 lodLevel, uint32 nodeIndex) const;
        MeshDeformerStack* GetMeshDeformerStack(uint32 lodLevel, uint32 nodeIndex) const;

        /** Finds the mesh points for which the specified node is the node with the highest influence.
         * This is a pretty expensive function which is only intended for use in the editor.
         * The resulting points will be given in model space.
         * @param node The node for which the most heavily influenced mesh points are sought.
         * @param[out] outPoints Container which will be filled with the points for which the node is the heaviest influence.
         */
        void FindMostInfluencedMeshPoints(const Node* node, AZStd::vector<AZ::Vector3>& outPoints) const;

        MCORE_INLINE Skeleton* GetSkeleton() const          { return mSkeleton; }
        MCORE_INLINE uint32 GetNumNodes() const             { return mSkeleton->GetNumNodes(); }

        void SetMesh(uint32 lodLevel, uint32 nodeIndex, Mesh* mesh);
        void SetMeshDeformerStack(uint32 lodLevel, uint32 nodeIndex, MeshDeformerStack* stack);

        bool CheckIfHasMorphDeformer(uint32 lodLevel, uint32 nodeIndex) const;
        bool CheckIfHasSkinningDeformer(uint32 lodLevel, uint32 nodeIndex) const;

        /**
         * Calculate the object oriented box for a given LOD level.
         * This will try to fit the tightest bounding box around the mesh of a node.
         * If the node has no mesh and acts as bone inside skinning deformations the resulting box will contain
         * all the vertices that are influenced by this given node/bone.
         * Calculating this box is already done at export time. But you can use this to recalculate it if the mesh data changed.
         * This method is relatively slow and not meant for per-frame calculations but only for preprocessing.
         * You can use the GetOBB() method to retrieve the calculated box at any time.
         * Nodes that do not have a mesh and not act as bone will have invalid OBB bounds, as they have no volume. You can check whether
         * this is the case or not by using the MCore::OBB::IsValid() method.
         * The box is stored in local space of the node.
         * @param lodLevel The geometry LOD level to generate the OBBs from.
         * @param nodeIndex The node to calculate the OBB for.
         */
        void CalcOBBFromBindPose(uint32 lodLevel, uint32 nodeIndex);

        /**
         * Get the object oriented bounding box for this node.
         * The box is in local space. In order to convert it into world space you have to multiply the corner points of the box
         * with the world space matrix of this node.
         * Nodes that do not have a mesh and do not act as bone will have invalid bounds. You can use the MCore::OBB::CheckIfIsValid() method to check if
         * the bounds are valid bounds or not. If it is not, then it means there was nothing to calculate the box from.
         * Object Oriented Boxes for the nodes are calculated at export time by using the Actor::UpdateNodeBindPoseOBBs() and Node::CalcOBBFromBindPose() methods.
         * @param nodeIndex The index of the node to get the OBB for.
         * @result The object oriented bounding box that has been calculated before already.
         */
        MCore::OBB& GetNodeOBB(uint32 nodeIndex)                            { return mNodeInfos[nodeIndex].mOBB; }

        /**
         * Get the object oriented bounding box for this node.
         * The box is in local space. In order to convert it into world space you have to multiply the corner points of the box
         * with the world space matrix of this node.
         * Nodes that do not have a mesh and do not act as bone will have invalid bounds. You can use the MCore::OBB::CheckIfIsValid() method to check if
         * the bounds are valid bounds or not. If it is not, then it means there was nothing to calculate the box from.
         * Object Oriented Boxes for the nodes are calculated at export time by using the Actor::UpdateNodeBindPoseOBBs() and Node::CalcOBBFromBindPose() methods.
         * @param nodeIndex The index of the node to get the OBB for.
         * @result The object oriented bounding box that has been calculated before already.
         */
        const MCore::OBB& GetNodeOBB(uint32 nodeIndex) const                { return mNodeInfos[nodeIndex].mOBB; }

        /**
         * Set the object oriented bounding box for this node.
         * The box is in local space. In order to convert it into world space you have to multiply the corner points of the box
         * with the world space matrix of this node.
         * Nodes that do not have a mesh and do not act as bone will have invalid bounds. You can use the MCore::OBB::CheckIfIsValid() method to check if
         * the bounds are valid bounds or not. If it is not, then it means there was nothing to calculate the box from.
         * @param nodeIndex The index of the node to set the OBB for.
         * @param obb The object oriented bounding box that has been calculated before already.
         */
        void SetNodeOBB(uint32 nodeIndex, const MCore::OBB& obb)            { mNodeInfos[nodeIndex].mOBB = obb; }

        void RemoveNodeMeshForLOD(uint32 lodLevel, uint32 nodeIndex, bool destroyMesh = true);

        void SetNumNodes(uint32 numNodes);

        void SetUnitType(MCore::Distance::EUnitType unitType);
        MCore::Distance::EUnitType GetUnitType() const;

        void SetFileUnitType(MCore::Distance::EUnitType unitType);
        MCore::Distance::EUnitType GetFileUnitType() const;

        EAxis FindBestMatchingMotionExtractionAxis() const;

        MCORE_INLINE uint32 GetRetargetRootNodeIndex() const    { return mRetargetRootNode; }
        MCORE_INLINE Node* GetRetargetRootNode() const          { return (mRetargetRootNode != MCORE_INVALIDINDEX32) ? mSkeleton->GetNode(mRetargetRootNode) : nullptr; }
        void SetRetargetRootNodeIndex(uint32 nodeIndex);
        void SetRetargetRootNode(Node* node);

        void AutoSetupSkeletalLODsBasedOnSkinningData(const AZStd::vector<AZStd::string>& alwaysIncludeJoints);
        void PrintSkeletonLODs();

        // Optimize a server version of the actor. The optimized skeleton will only have critical joints, hit detection collider joints and all their ancestor joints.
        void GenerateOptimizedSkeleton();

        void SetOptimizeSkeleton(bool optimizeSkeleton) { m_optimizeSkeleton = optimizeSkeleton; }
        bool GetOptimizeSkeleton() const { return m_optimizeSkeleton; }

        void SetMeshAssetId(const AZ::Data::AssetId& assetId);
        AZ::Data::AssetId GetMeshAssetId() const { return m_meshAssetId; };

        const AZ::Data::Asset<AZ::RPI::ModelAsset>& GetMeshAsset() const { return m_meshAsset; }
        const AZ::Data::Asset<AZ::RPI::SkinMetaAsset>& GetSkinMetaAsset() const { return m_skinMetaAsset; }
        const AZ::Data::Asset<AZ::RPI::MorphTargetMetaAsset>& GetMorphTargetMetaAsset() const { return m_morphTargetMetaAsset; }
        const AZStd::unordered_map<AZ::u16, AZ::u16>& GetSkinToSkeletonIndexMap() const { return m_skinToSkeletonIndexMap; }

        /**
         * Is the actor fully ready?
         * @result True in case the actor as well as its dependent files (e.g. mesh, skin, morph targets) are fully loaded and initialized.
         **/
        bool IsReady() const { return m_isReady; }

        /**
         * Finalize the actor with preload assets (mesh, skinmeta and morph target assets).
         * LoadRequirement - We won't need a blocking load if the actor is part of the actor asset, as that will trigger the preload assets
         * to load and get ready before finalize has been reached.
         * However, if we are calling this on an actor that bypassed the asset system (e.g loading the actor directly from disk), it will require
         * a blocking load. This option is now being used because emfx editor does not fully integrate with the asset system.
         */
        void Finalize(LoadRequirement loadReq = LoadRequirement::AllowAsyncLoad);

    private:
        void InsertJointAndParents(AZ::u32 jointIndex, AZStd::unordered_set<AZ::u32>& includedJointIndices);

        AZStd::unordered_map<AZ::u16, AZ::u16> ConstructSkinToSkeletonIndexMap(const AZ::Data::Asset<AZ::RPI::SkinMetaAsset>& skinMetaAsset);
        void ConstructMeshes();
        void ConstructMorphTargets();

        Node* FindJointByMeshName(const AZStd::string_view meshName) const;

        // per node info (shared between lods)
        struct EMFX_API NodeInfo
        {
            MCore::OBB  mOBB;

            NodeInfo();
        };

        // data per node, per lod
        struct EMFX_API NodeLODInfo
        {
            Mesh*                   mMesh;
            MeshDeformerStack*      mStack;

            NodeLODInfo();
            ~NodeLODInfo();
        };

        // a lod level
        struct EMFX_API LODLevel
        {
            MCore::Array<NodeLODInfo> mNodeInfos;

            LODLevel();
        };

        struct MeshLODData
        {
            AZStd::vector<LODLevel> m_lodLevels;

            MeshLODData();
        };

        MeshLODData m_meshLodData;
        AZ::Data::AssetId m_meshAssetId;
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_meshAsset;
        AZ::Data::Asset<AZ::RPI::SkinMetaAsset> m_skinMetaAsset;
        AZ::Data::Asset<AZ::RPI::MorphTargetMetaAsset> m_morphTargetMetaAsset;
        AZStd::recursive_mutex m_mutex;
        AZStd::unordered_map<AZ::u16, AZ::u16> m_skinToSkeletonIndexMap; //!< Mapping joint indices in skin metadata to skeleton indices.

        static AZ::Data::AssetId ConstructSkinMetaAssetId(const AZ::Data::AssetId& meshAssetId);
        static bool DoesSkinMetaAssetExist(const AZ::Data::AssetId& meshAssetId);

        static AZ::Data::AssetId ConstructMorphTargetMetaAssetId(const AZ::Data::AssetId& meshAssetId);
        static bool DoesMorphTargetMetaAssetExist(const AZ::Data::AssetId& meshAssetId);

        Node* FindMeshJoint(const AZ::Data::Asset<AZ::RPI::ModelLodAsset>& lodModelAsset) const;

        Skeleton*                                       mSkeleton;                  /**< The skeleton, containing the nodes and bind pose. */
        MCore::Array<Dependency>                        mDependencies;              /**< The dependencies on other actors (shared meshes and transforms). */
        AZStd::vector<NodeInfo>                         mNodeInfos;                 /**< The per node info, shared between lods. */
        AZStd::string                                   mName;                      /**< The name of the actor. */
        AZStd::string                                   mFileName;                  /**< The filename of the actor. */
        MCore::Array<NodeMirrorInfo>                    mNodeMirrorInfos;           /**< The array of node mirror info. */
        MCore::Array< MCore::Array< Material* > >       mMaterials;                 /**< A collection of materials (for each lod). */
        MCore::Array< MorphSetup* >                     mMorphSetups;               /**< A morph setup for each geometry LOD. */
        MCore::SmallArray<NodeGroup*>                   mNodeGroups;                /**< The set of node groups. */
        AZStd::shared_ptr<PhysicsSetup>                 m_physicsSetup;             /**< Hit detection, ragdoll and cloth colliders, joint limits and rigid bodies. */
        AZStd::shared_ptr<SimulatedObjectSetup>         m_simulatedObjectSetup;     /**< Setup for simulated objects */
        MCore::Distance::EUnitType                      mUnitType;                  /**< The unit type used on export. */
        MCore::Distance::EUnitType                      mFileUnitType;              /**< The unit type used on export. */
        AZStd::vector<Transform>                        mInvBindPoseTransforms;     /**< The inverse world space bind pose transforms. */
        void*                                           mCustomData;                /**< Some custom data, for example a pointer to your own game character class which is linked to this actor. */
        uint32                                          mMotionExtractionNode;      /**< The motion extraction node. This is the node from which to transfer a filtered part of the motion onto the actor instance. Can also be MCORE_INVALIDINDEX32 when motion extraction is disabled. */
        uint32                                          mRetargetRootNode;          /**< The retarget root node, which controls the height displacement of the character. This is most likely the hip or pelvis node. */
        uint32                                          mID;                        /**< The unique identification number for the actor. */
        uint32                                          mThreadIndex;               /**< The thread number we are running on, which is a value starting at 0, up to the number of threads in the job system. */
        MCore::AABB                                     mStaticAABB;                /**< The static AABB. */
        bool                                            mDirtyFlag;                 /**< The dirty flag which indicates whether the user has made changes to the actor since the last file save operation. */
        bool                                            mUsedForVisualization;      /**< Indicates if the actor is used for visualization specific things and is not used as a normal in-game actor. */
        bool                                            m_optimizeSkeleton;         /**< Indicates if we should perform/ */
        bool                                            m_isReady = false;          /**< If actor as well as its dependent files are fully loaded and initialized.*/
#if defined(EMFX_DEVELOPMENT_BUILD)
        bool                                            mIsOwnedByRuntime;          /**< Set if the actor is used/owned by the engine runtime. */
#endif // EMFX_DEVELOPMENT_BUILD
    };
} // namespace EMotionFX
