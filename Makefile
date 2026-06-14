NAME    = TeslaViewer
SRCDIR  = src
OBJDIR  = obj

SRCS    = $(wildcard $(SRCDIR)/*.cpp)
OBJS    = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))

CXX      = g++
CXXFLAGS = -Wall -O2 -std=c++17 -I$(SRCDIR) -MMD -MP
LDFLAGS  = -lbe -lmedia -ltracker -ltranslation -lroot

DEPS    = $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS) $(NAME).rdef
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)
	rc -o $(NAME).rsrc $(NAME).rdef
	xres -o $@ $(NAME).rsrc
	mimeset -f $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(NAME) $(NAME).rsrc

run: $(NAME)
	./$(NAME)

.PHONY: all clean run
