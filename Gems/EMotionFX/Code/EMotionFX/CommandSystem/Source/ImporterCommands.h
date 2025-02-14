/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#ifndef __EMFX_IMPORTERCOMMANDS_H
#define __EMFX_IMPORTERCOMMANDS_H

// include the required headers
#include "CommandSystemConfig.h"
#include <MCore/Source/Command.h>
#include <EMotionFX/Source/Importer/Importer.h>
#include "CommandManager.h"


namespace CommandSystem
{
    // add actor
    MCORE_DEFINECOMMAND_START(CommandImportActor, "Import actor", true)
public:
    uint32  mPreviouslyUsedID;
    uint32  mOldIndex;
    bool    mOldWorkspaceDirtyFlag;
    MCORE_DEFINECOMMAND_END

    // add motion
        MCORE_DEFINECOMMAND_START(CommandImportMotion, "Import motion", true)
public:
    uint32          mOldMotionID;
    AZStd::string   mOldFileName;
    bool            mOldWorkspaceDirtyFlag;
    MCORE_DEFINECOMMAND_END

} // namespace CommandSystem


#endif
