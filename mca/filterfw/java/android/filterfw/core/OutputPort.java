/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


package android.filterfw.core;

public class OutputPort extends SourcePort {

    public OutputPort(Filter filter, String name) {
        super(filter, name);
    }

    @Override
    public void clear() {
        if (mTargetPort != null) {
            mTargetPort.clear();
        }
    }

    @Override
    public void pushFrame(Frame frame) {
        if (mTargetPort == null) {
            throw new RuntimeException(
                "Attempting to push frame on unconnected port: " + this + "!");
        }
        mTargetPort.pushFrame(frame);
    }

    @Override
    public void setFrame(Frame frame) {
        assertPortIsOpen();
        if (mTargetPort == null) {
            throw new RuntimeException(
                "Attempting to set frame on unconnected port: " + this + "!");
        }
        mTargetPort.setFrame(frame);
    }

    @Override
    public Frame pullFrame() {
        throw new RuntimeException("Cannot pull frame on " + this + "!");
    }

    @Override
    public boolean hasFrame() {
        return mTargetPort == null ? false : mTargetPort.hasFrame();
    }

    @Override
    public String toString() {
        return "output " + super.toString();
    }
}