Repository Structure

The repository is organized primarily around the assistant's core subsystems.

Nova-AI/
│
├── nova.py
│
├── core/
│ ├── llm_client.py
│ ├── runtime_entrygate.py
│ ├── input_router.py
│ ├── conversational_runtime.py
│ ├── conversation_handler.py
│ │
│ ├── nova_runtime.py
│ ├── runtime_context.py
│ ├── runtime_state.py
│ ├── runtime_trace.py
│ ├── runtime_events.py
│ │
│ ├── planner_pipeline.py
│ ├── plan_parser.py
│ ├── plan_quality_analyzer.py
│ ├── planner_confidence.py
│ ├── plan_normalizer.py
│ ├── intent_expander.py
│ ├── plan_validator.py
│ ├── plan_repair_engine.py
│ │
│ ├── task_translator.py
│ ├── execution_policy.py
│ ├── execution_router.py
│ ├── execution_verifier.py
│ ├── recovery_engine.py
│ │
│ ├── application_handler.py
│ ├── filesystem_handler.py
│ ├── terminal_handler.py
│ ├── terminal_session.py
│ ├── git_handler.py
│ ├── browser_handler.py
│ │
│ ├── wake_local.py
│ ├── voice.py
│ │
│ ├── vision_engine.py
│ ├── screen_capture.py
│ ├── ocr_reader.py
│ ├── system_state.py
│ ├── reasoning_engine.py
│ ├── context_fusion_engine.py
│ │
│ └── ...
│
├── ui/
│ └── orb.py
│
├── plugins/
│
├── OmniParser/
│
├── tests/
│
└── README.md

Some prototype and experimental components remain in the repository because they are candidates for future integration.
