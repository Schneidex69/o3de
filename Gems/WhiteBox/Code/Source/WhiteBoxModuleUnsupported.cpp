/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <WhiteBoxUnsupported_precompiled.h>

#include <AzCore/Module/Module.h>

#if defined(WHITE_BOX_EDITOR)
AZ_DECLARE_MODULE_CLASS(Gem_WhiteBoxEditorModule, AZ::Module)
#else
AZ_DECLARE_MODULE_CLASS(Gem_WhiteBox, AZ::Module)
#endif
