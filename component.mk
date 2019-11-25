ARDUINO_ALL_LIBRARIES := $(patsubst $(COMPONENT_PATH)/libraries/%,%,$(wildcard $(COMPONENT_PATH)/libraries/*))

# Macro returns non-empty if Arduino library $(1) should be included in the build
# (either because selective compilation is of, or this library is enabled
define ARDUINO_LIBRARY_ENABLED
$(if $(CONFIG_ARDUINO_SELECTIVE_COMPILATION),$(CONFIG_ARDUINO_SELECTIVE_$(1)),y)
endef

ARDUINO_ENABLED_LIBRARIES := $(foreach LIBRARY,$(sort $(ARDUINO_ALL_LIBRARIES)),$(if $(call ARDUINO_LIBRARY_ENABLED,$(LIBRARY)),$(LIBRARY)))

$(info Arduino libraries in build: $(ARDUINO_ENABLED_LIBRARIES))

# Expand all subdirs under $(1)
define EXPAND_SUBDIRS
$(sort $(dir $(wildcard $(1)/* $(1)/*/* $(1)/*/*/* $(1)/*/*/*/* $(1)/*/*/*/*/*)))
endef

# Macro returns SRCDIRS for library
define ARDUINO_LIBRARY_GET_SRCDIRS
	$(if $(wildcard $(COMPONENT_PATH)/libraries/$(1)/src/.), 							\
		$(call EXPAND_SUBDIRS,$(COMPONENT_PATH)/libraries/$(1)/src), 					\
		$(filter-out $(call EXPAND_SUBDIRS,$(COMPONENT_PATH)/libraries/$(1)/examples),  \
			$(call EXPAND_SUBDIRS,$(COMPONENT_PATH)/libraries/$(1)) 				   	\
		) 																				\
	)
endef

# Make a list of all srcdirs in enabled libraries
ARDUINO_LIBRARY_SRCDIRS := $(patsubst $(COMPONENT_PATH)/%,%,$(foreach LIBRARY,$(ARDUINO_ENABLED_LIBRARIES),$(call ARDUINO_LIBRARY_GET_SRCDIRS,$(LIBRARY))))

#$(info Arduino libraries src dirs: $(ARDUINO_LIBRARY_SRCDIRS))

COMPONENT_ADD_INCLUDEDIRS := cores/esp32 variants/esp32 $(ARDUINO_LIBRARY_SRCDIRS)
COMPONENT_PRIV_INCLUDEDIRS := cores/esp32/libb64
COMPONENT_SRCDIRS := cores/esp32/libb64 cores/esp32 variants/esp32 $(ARDUINO_LIBRARY_SRCDIRS)
CXXFLAGS += -fno-rtti
