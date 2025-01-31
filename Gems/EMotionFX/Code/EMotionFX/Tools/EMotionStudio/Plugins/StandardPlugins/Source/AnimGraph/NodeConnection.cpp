/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// include the required headers
#include "NodeConnection.h"
#include "NodeGraph.h"
#include "GraphNode.h"
#include <MCore/Source/LogManager.h>
#include <MCore/Source/Vector.h>


namespace EMStudio
{
    // constructor
    NodeConnection::NodeConnection(NodeGraph* parentGraph, const QModelIndex& modelIndex, GraphNode* targetNode, uint32 portNr, GraphNode* sourceNode, uint32 sourceOutputPortNr)
        : m_modelIndex(modelIndex)
        , m_parentGraph(parentGraph)
    {
        mSourceNode     = sourceNode;
        mSourcePortNr   = sourceOutputPortNr;
        mTargetNode     = targetNode;
        mPortNr         = portNr;
        mIsVisible      = false;
        mIsProcessed    = false;
        mIsDashed       = false;
        mIsDisabled     = false;
        mIsHeadHighlighted = false;
        mIsTailHighlighted = false;
        mIsSynced       = false;
        mColor          = QColor(128, 255, 128);
    }


    // destructor
    NodeConnection::~NodeConnection()
    {
    }


    // update the connection
    void NodeConnection::Update(const QRect& visibleRect, const QPoint& mousePos)
    {
        MCORE_UNUSED(mousePos);

        // calculate the rects
        mRect       = CalcRect();
        mFinalRect  = CalcFinalRect();

        // check for visibility
        mIsVisible  = mFinalRect.intersects(visibleRect);

        // reset the is highlighted flags
        mIsHighlighted          = false;
        mIsConnectedHighlighted = false;
    }


    // update the painter path
    void NodeConnection::UpdatePainterPath()
    {
        const QRect sourceRect = GetSourceRect();
        const QRect targetRect = GetTargetRect();

        // get the start and end coordinates for the connections
        int32 startX    = sourceRect.center().x();
        int32 endX      = targetRect.center().x();
        int32 startY    = sourceRect.center().y() + 1;
        int32 endY      = targetRect.center().y() + 1;

        // draw the connection
        mPainterPath = QPainterPath();
        const float width = aznumeric_cast<float>(abs((endX - 3) - (startX + 3)));
        mPainterPath.moveTo(startX, startY);
        mPainterPath.lineTo(startX + 3, startY);
        mPainterPath.cubicTo(startX + (width / 2), startY, endX - (width / 2), endY, endX - 3, endY);
        mPainterPath.lineTo(endX, endY);
    }


    // render the connection
    void NodeConnection::Render([[maybe_unused]] const QItemSelectionModel& selectionModel, QPainter& painter, QPen* pen, QBrush* brush, int32 stepSize, const QRect& visibleRect, float opacity, bool alwaysColor)
    {
        AZ_UNUSED(brush);
        AZ_UNUSED(stepSize);
        AZ_UNUSED(visibleRect);

        // used when relinking
        if (mIsDashed)
        {
            return;
        }

        painter.setOpacity(opacity);

        const float scale = mSourceNode->GetParentGraph()->GetScale();
        QColor penColor;

        // draw some small horizontal lines that go outside of the connection port
        if (GetIsSelected())
        {
            penColor.setRgb(255, 128, 0);

            if (scale > 0.75f)
            {
                pen->setWidth(2);
            }
            else
            {
                pen->setWidth(1);
            }
        }
        else // unselected
        {
            // don't make it bold when not selected
            if (mIsProcessed == false && alwaysColor == false)
            {
                if (mSourceNode)
                {
                    //penColor = mSourceNode->GetOutputPort(mSourcePortNr)->GetColor();
                    penColor.setRgb(75, 75, 75);
                }
                else
                {
                    uint32 col = aznumeric_cast<uint32>(scale * 200);
                    col = MCore::Clamp<uint32>(col, 130, 130);
                    penColor.setRgb(col, col, col);
                }
            }
            else
            {
                if (mSourceNode)
                {
                    if (alwaysColor == false)
                    {
                        pen->setWidthF(1.5f);
                    }

                    penColor = mSourceNode->GetOutputPort(mSourcePortNr)->GetColor();
                }
                else
                {
                    penColor.setRgb(75, 75, 75);
                }
            }
            //penColor.setRgb(255,225,0);
            //          penColor.setRgb(255,0,255);
        }

        // lighten the color in case the transition is highlighted
        if (mIsHighlighted)
        {
            penColor = penColor.lighter(160);
        }

        // lighten the color in case the transition is connected to the currently selected node
        if (mIsConnectedHighlighted)
        {
            const float minInput = 0.1f;
            const float maxInput = 1.0f;
            const float minOutput = 3.0;
            const float maxOutput = 1.0;

            // clamp it so that the value is in the valid input range
            float x = MCore::Clamp<float>(scale, minInput, maxInput);

            // apply the simple linear conversion
            float result;
            if (MCore::Math::Abs(maxInput - minInput) > MCore::Math::epsilon)
            {
                result = ((x - minInput) / (maxInput - minInput)) * (maxOutput - minOutput) + minOutput;
            }
            else
            {
                result = minOutput;
            }

            pen->setWidth(aznumeric_cast<int>(result));
            penColor = penColor.lighter(160);
        }

        // blinking red error color
        if (mSourceNode && mSourceNode->GetHasError() && !GetIsSelected())
        {
            NodeGraph* parentGraph = mTargetNode->GetParentGraph();
            if (parentGraph->GetUseAnimation())
            {
                penColor = parentGraph->GetErrorBlinkColor();
            }
            else
            {
                penColor = Qt::red;
            }
        }

        // set the pen
        pen->setColor(penColor);
        if (mIsProcessed)
        {
            NodeGraph* parentGraph = mTargetNode->GetParentGraph();
            if (parentGraph->GetScale() > 0.5f && parentGraph->GetUseAnimation())
            {
                pen->setStyle(Qt::PenStyle::DashLine);
                pen->setDashOffset(parentGraph->GetDashOffset());
            }
            else
            {
                pen->setStyle(Qt::PenStyle::SolidLine);
                //pen->setDashOffset( -parentGraph->GetDashOffset() );
            }

            pen->setWidth(2);
        }
        else
        {
            pen->setStyle(Qt::SolidLine);
        }

        painter.setPen(*pen);

        // set the brush
        //brush->setColor(penColor);
        //painter.setBrush( *brush );
        painter.setBrush(Qt::NoBrush);

        // draw the curve
        UpdatePainterPath();
        painter.drawPath(mPainterPath);

        // restore opacity and width
        painter.setOpacity(1.0f);
        pen->setWidth(1);
    }


    // get the source rect
    QRect NodeConnection::GetSourceRect() const
    {
        if (mSourceNode)
        {
            if (mSourceNode->GetIsCollapsed() == false)
            {
                return mSourceNode->GetOutputPort(mSourcePortNr)->GetRect();
            }
            else
            {
                return CalcCollapsedSourceRect();
            }
        }

        QRect defaultRect = GetTargetRect();
        defaultRect.setLeft(defaultRect.left() - WILDCARDTRANSITION_SIZE);
        defaultRect.setTop(defaultRect.top() - WILDCARDTRANSITION_SIZE);
        return defaultRect;
    }


    // get the target rect
    QRect NodeConnection::GetTargetRect() const
    {
        if (mTargetNode->GetIsCollapsed() == false)
        {
            return mTargetNode->GetInputPort(mPortNr)->GetRect();
        }
        else
        {
            return CalcCollapsedTargetRect();
        }
    }


    // intersects this connection?
    bool NodeConnection::Intersects(const QRect& rect)
    {
        if (mRect.intersects(rect) == false)
        {
            return false;
        }

        // get the port rects
        //QRect sourceRect = GetSourceRect();
        //QRect targetRect = GetTargetRect();

        //return NodeGraph::RectIntersectsSmoothedLine(rect, sourceRect.center().x(), sourceRect.center().y(), targetRect.center().x(), targetRect.center().y());

        //QPainterPath testPath;
        //testPath.addRect( rect );

        UpdatePainterPath();
        return mPainterPath.intersects(rect);
    }


    // is it close to this connection?
    bool NodeConnection::CheckIfIsCloseTo(const QPoint& point)
    {
        // if we're not visible don't check
        if (mIsVisible == false)
        {
            return false;
        }

        if (mRect.contains(point) == false)
        {
            return false;
        }

        // get the port rects
        //QRect sourceRect = GetSourceRect();
        //QRect targetRect = GetTargetRect();

        //return NodeGraph::PointCloseToSmoothedLine(sourceRect.center().x(), sourceRect.center().y(), targetRect.center().x(), targetRect.center().y(), point.x(), point.y());

        int32 size = 6;
        int32 halfSize = size / 2;
        QRect testRect(point.x() - halfSize, point.y() - halfSize, size, size);
        return Intersects(testRect);
    }


    // get the collapsed source rect
    QRect NodeConnection::CalcCollapsedSourceRect() const
    {
        QRect tempRect = mSourceNode->GetRect();
        //  QPoint a = QPoint(tempRect.right(), tempRect.top() + tempRect.height() / 2);
        QPoint a = QPoint(tempRect.right(), tempRect.top() + 13);
        return QRect(a - QPoint(1, 1), a);
    }


    // get the collapsed target rect
    QRect NodeConnection::CalcCollapsedTargetRect() const
    {
        QRect tempRect = mTargetNode->GetRect();
        //  QPoint a = QPoint(tempRect.left(), tempRect.top() + tempRect.height() / 2);
        QPoint a = QPoint(tempRect.left(), tempRect.top() + 13);
        return QRect(a, a + QPoint(1, 1));
    }


    // calculate the bounds
    QRect NodeConnection::CalcRect() const
    {
        QRect sourceRect = GetSourceRect();
        QRect targetRect = GetTargetRect();
        return sourceRect.united(targetRect);
    }


    // calc the final rect
    QRect NodeConnection::CalcFinalRect() const
    {
        if (mSourceNode)
        {
            return mSourceNode->GetParentGraph()->GetTransform().mapRect(CalcRect());
        }

        if (mTargetNode)
        {
            return mTargetNode->GetParentGraph()->GetTransform().mapRect(CalcRect());
        }

        MCORE_ASSERT(false);
        return QRect();
    }

    bool NodeConnection::GetIsSelected() const
    {
        return m_parentGraph->GetAnimGraphModel().GetSelectionModel().isSelected(m_modelIndex);
    }
}   // namespace EMotionFX
