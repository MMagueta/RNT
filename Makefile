CXX = cl
TARGET = RNT.exe
OBJECTS = NT.obj ObjectManager.obj PermissionsManager.obj NamespaceReferenceManager.obj CursorManager.obj IdentityManager.obj LifecycleManager.obj HandlerManager.obj VM.obj
INCDIR = include
DEFINES = /DNT_CONSOLE_APP /DNT_WAIT_ON_EXIT
INCLUDE_FLAG = /I$(INCDIR)
COMPILE_FLAGS = /std:c++17 /W4 /EHsc
COMPILE_ONLY = /c
LINK_FLAGS =
TARGET_OUT = /Fe:$(TARGET)
REMOVE = del /Q

all: $(TARGET)

windows: all

unix:
	$(MAKE) CXX=c++ TARGET=RNT OBJECTS="NT.o ObjectManager.o PermissionsManager.o NamespaceReferenceManager.o CursorManager.o IdentityManager.o LifecycleManager.o HandlerManager.o VM.o" DEFINES=-DNT_CONSOLE_APP INCLUDE_FLAG=-Iinclude COMPILE_FLAGS="-std=c++17 -Wall -Wextra" COMPILE_ONLY=-c LINK_FLAGS= TARGET_OUT="-o RNT" REMOVE="rm -f" all

linux: unix

macos:
	$(MAKE) CXX=c++ TARGET=RNT OBJECTS="NT.o ObjectManager.o PermissionsManager.o NamespaceReferenceManager.o CursorManager.o IdentityManager.o LifecycleManager.o HandlerManager.o VM.o" DEFINES=-DNT_CONSOLE_APP INCLUDE_FLAG=-Iinclude COMPILE_FLAGS="-std=c++17 -Wall -Wextra" COMPILE_ONLY=-c LINK_FLAGS= TARGET_OUT="-o RNT" REMOVE="rm -f" all

$(TARGET): $(OBJECTS)
	$(CXX) $(LINK_FLAGS) $(OBJECTS) $(TARGET_OUT)

NT.obj: src\NT.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src\NT.cpp /Fo:NT.obj

ObjectManager.obj: src\ObjectManager.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src\ObjectManager.cpp /Fo:ObjectManager.obj

PermissionsManager.obj: src\PermissionsManager.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src\PermissionsManager.cpp /Fo:PermissionsManager.obj

NamespaceReferenceManager.obj: src\NamespaceReferenceManager.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src\NamespaceReferenceManager.cpp /Fo:NamespaceReferenceManager.obj

CursorManager.obj: src\CursorManager.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src\CursorManager.cpp /Fo:CursorManager.obj

IdentityManager.obj: src\IdentityManager.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src\IdentityManager.cpp /Fo:IdentityManager.obj

LifecycleManager.obj: src\LifecycleManager.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src\LifecycleManager.cpp /Fo:LifecycleManager.obj

HandlerManager.obj: src\HandlerManager.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src\HandlerManager.cpp /Fo:HandlerManager.obj

VM.obj: src\VM.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src\VM.cpp /Fo:VM.obj

NT.o: src/NT.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src/NT.cpp -o NT.o

ObjectManager.o: src/ObjectManager.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src/ObjectManager.cpp -o ObjectManager.o

PermissionsManager.o: src/PermissionsManager.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src/PermissionsManager.cpp -o PermissionsManager.o

NamespaceReferenceManager.o: src/NamespaceReferenceManager.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src/NamespaceReferenceManager.cpp -o NamespaceReferenceManager.o

CursorManager.o: src/CursorManager.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src/CursorManager.cpp -o CursorManager.o

IdentityManager.o: src/IdentityManager.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src/IdentityManager.cpp -o IdentityManager.o

LifecycleManager.o: src/LifecycleManager.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src/LifecycleManager.cpp -o LifecycleManager.o

HandlerManager.o: src/HandlerManager.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src/HandlerManager.cpp -o HandlerManager.o

VM.o: src/VM.cpp
	$(CXX) $(DEFINES) $(INCLUDE_FLAG) $(COMPILE_FLAGS) $(COMPILE_ONLY) src/VM.cpp -o VM.o

clean:
	-$(REMOVE) RNT.exe RNT $(OBJECTS) NT.obj ObjectManager.obj PermissionsManager.obj NamespaceReferenceManager.obj CursorManager.obj IdentityManager.obj LifecycleManager.obj HandlerManager.obj VM.obj NT.o ObjectManager.o PermissionsManager.o NamespaceReferenceManager.o CursorManager.o IdentityManager.o LifecycleManager.o HandlerManager.o VM.o RNT.exp RNT.lib
