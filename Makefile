!IF "$(PLATFORM)"=="X64" || "$(PLATFORM)"=="x64"
ARCH=amd64
!ELSE
ARCH=x86
!ENDIF

GTEST_SRC_DIR=D:\src\googletest
GTEST_BUILD_DIR=D:\src\googletest\build\$(ARCH)

OUTDIR=bin\$(ARCH)
OBJDIR=obj\$(ARCH)
SRCDIR=src

CC=cl
RD=rd/s/q
RM=del/q
LINKER=link
TARGET=la.exe
TARGET_NESTED=nested.exe
TEST_TARGET=t.exe

OBJS=\
	$(OBJDIR)\args.obj\
	$(OBJDIR)\blob.obj\
	$(OBJDIR)\main.obj\

OBJS_NESTED=\
	$(OBJDIR)\nestedmain.obj\

TEST_OBJS=\
	$(OBJDIR)\args.obj\
	$(OBJDIR)\testmain.obj\

LIBS=\
	comsuppw.lib\
	user32.lib\

TEST_LIBS=\
	gtest.lib\
	gtest_main.lib\

CFLAGS=\
	/nologo\
	/c\
	/DUNICODE\
	/D_CRT_SECURE_NO_WARNINGS\
#	/DDOWNLEVEL\
	/Od\
	/W4\
	/Zi\
	/EHsc\
	/Fo"$(OBJDIR)\\"\
	/Fd"$(OBJDIR)\\"\
	/I"$(GTEST_SRC_DIR)\googletest\include"\
	/I"$(GTEST_SRC_DIR)\googlemock\include"\

LFLAGS=\
	/NOLOGO\
	/DEBUG\
	/LIBPATH:"$(GTEST_BUILD_DIR)\lib\Release"\

all: $(OUTDIR)\$(TARGET) $(OUTDIR)\$(TARGET_NESTED) $(OUTDIR)\$(TEST_TARGET)

$(OUTDIR)\$(TARGET): $(OBJS)
	@if not exist $(OUTDIR) mkdir $(OUTDIR)
	$(LINKER) $(LFLAGS) /SUBSYSTEM:CONSOLE $(LIBS) /PDB:"$(@R).pdb" /OUT:$@ $**

$(OUTDIR)\$(TARGET_NESTED): $(OBJS_NESTED)
	@if not exist $(OUTDIR) mkdir $(OUTDIR)
	$(LINKER) $(LFLAGS) /SUBSYSTEM:WINDOWS $(LIBS) /PDB:"$(@R).pdb" /OUT:$@ $**

$(OUTDIR)\$(TEST_TARGET): $(TEST_OBJS)
	@if not exist $(OUTDIR) mkdir $(OUTDIR)
	$(LINKER) $(LFLAGS) /SUBSYSTEM:CONSOLE $(TEST_LIBS) /PDB:"$(@R).pdb" /OUT:$@ $**

{$(SRCDIR)}.cpp{$(OBJDIR)}.obj:
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
	$(CC) $(CFLAGS) $<

clean:
	@if exist $(OBJDIR) $(RD) $(OBJDIR)
	@if exist $(OUTDIR)\$(TARGET) $(RM) $(OUTDIR)\$(TARGET)
	@if exist $(OUTDIR)\$(TARGET:exe=ilk) $(RM) $(OUTDIR)\$(TARGET:exe=ilk)
	@if exist $(OUTDIR)\$(TARGET:exe=pdb) $(RM) $(OUTDIR)\$(TARGET:exe=pdb)
	@if exist $(OUTDIR)\$(TEST_TARGET) $(RM) $(OUTDIR)\$(TEST_TARGET)
	@if exist $(OUTDIR)\$(TEST_TARGET:exe=ilk) $(RM) $(OUTDIR)\$(TEST_TARGET:exe=ilk)
	@if exist $(OUTDIR)\$(TEST_TARGET:exe=pdb) $(RM) $(OUTDIR)\$(TEST_TARGET:exe=pdb)
