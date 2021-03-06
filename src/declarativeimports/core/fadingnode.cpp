/*
 * Copyright (C) 2014  David Edmundson <davidedmundson@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "fadingnode_p.h"

#include <QSGSimpleMaterialShader>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

struct FadingMaterialState
{
   QSGTexture *source;
   QSGTexture *target;
   qreal progress;
};

class FadingMaterialShader : public QSGSimpleMaterialShader<FadingMaterialState>
{
    QSG_DECLARE_SIMPLE_SHADER(FadingMaterialShader, FadingMaterialState)
public:
    virtual const char* fragmentShader() const;
    virtual const char* vertexShader() const;

    using QSGSimpleMaterialShader<FadingMaterialState>::updateState;
    virtual void updateState(const FadingMaterialState* newState, const FadingMaterialState* oldState) override;
    virtual QList<QByteArray> attributes() const;

    virtual void initialize();
private:
    QOpenGLFunctions *glFuncs = 0;
    int m_progressId = 0;
};

QList<QByteArray> FadingMaterialShader::attributes() const
{
    return QList<QByteArray>() << "qt_Vertex" << "qt_MultiTexCoord0";
}

const char* FadingMaterialShader::vertexShader() const
{
     return "uniform highp mat4 qt_Matrix;"
            "attribute highp vec4 qt_Vertex;"
            "attribute highp vec2 qt_MultiTexCoord0;"
            "varying highp vec2 v_coord;"
            "void main() {"
            "        v_coord = qt_MultiTexCoord0;"
            "        gl_Position = qt_Matrix * qt_Vertex;"
            "    }";
}

const char* FadingMaterialShader::fragmentShader() const
{
    return "varying highp vec2 v_coord;"
    "uniform sampler2D u_src;"
    "uniform sampler2D u_target;"
    "uniform highp float u_transitionProgress;"
    "uniform lowp float qt_Opacity;"
    "void main() {"
        "lowp vec4 tex1 = texture2D(u_target, v_coord);"
        "lowp vec4 tex2 = texture2D(u_src, v_coord);"
        "gl_FragColor = mix(tex2, tex1, u_transitionProgress) * qt_Opacity;"
    "}";
}


void FadingMaterialShader::updateState(const FadingMaterialState* newState, const FadingMaterialState* oldState)
{
    if (!oldState || oldState->source != newState->source) {
        glFuncs->glActiveTexture(GL_TEXTURE1);
        newState->source->bind();
        // reset the active texture back to 0 after we changed it to something else
        glFuncs->glActiveTexture(GL_TEXTURE0);
    }

    if (!oldState || oldState->target != newState->target) {
        glFuncs->glActiveTexture(GL_TEXTURE0);
        newState->target->bind();
    }

    if (!oldState || oldState->progress != newState->progress) {
        program()->setUniformValue(m_progressId, (GLfloat) newState->progress);
    }
}

void FadingMaterialShader::initialize()
{
    if (!program()->isLinked()) {
        // shader not linked, exit otherwise we crash, BUG: 336272
        return;
    }
    QSGSimpleMaterialShader< FadingMaterialState >::initialize();
    glFuncs = QOpenGLContext::currentContext()->functions();
    program()->bind();
    program()->setUniformValue("u_src", 0);
    program()->setUniformValue("u_target", 1);
    m_progressId = program()->uniformLocation("u_transitionProgress");
}


FadingNode::FadingNode(QSGTexture *source, QSGTexture *target):
    m_source(source),
    m_target(target)
{
    QSGSimpleMaterial<FadingMaterialState> *m = FadingMaterialShader::createMaterial();
    m->setFlag(QSGMaterial::Blending);
    setMaterial(m);
    setFlag(OwnsMaterial, true);
    setProgress(1.0);

    QSGGeometry *g = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
    QSGGeometry::updateTexturedRectGeometry(g, QRect(), QRect());
    setGeometry(g);
    setFlag(QSGNode::OwnsGeometry, true);
}

FadingNode::~FadingNode()
{
}

void FadingNode::setRect(const QRectF &bounds)
{
    QSGGeometry::updateTexturedRectGeometry(geometry(), bounds, QRectF(0, 0, 1, 1));
    markDirty(QSGNode::DirtyGeometry);
}

void FadingNode::setProgress(qreal progress)
{
    QSGSimpleMaterial<FadingMaterialState> *m = static_cast<QSGSimpleMaterial<FadingMaterialState>*>(material());
    m->state()->source = m_source.data();
    m->state()->target = m_target.data();
    m->state()->progress = progress;
    markDirty(QSGNode::DirtyMaterial);
}
