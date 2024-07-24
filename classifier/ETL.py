
import os
import re

def extract_and_save_sections(input_file_path, output_dir):
    with open(input_file_path, 'r') as file:
        content = file.read()

    # Define regex patterns for different sections and their labels
    patterns = {
        "preparing_env": r"[*]+ Preparing the environment for Debugger Extensions Gallery repositories [*]+\n.*?(?=\n[*]+|$)",
        "waiting_for_debugger": r"[*]+ Waiting for Debugger Extensions Gallery to Initialize [*]+\n.*?(?=\n[*]+|$)",
        "path_validation_summary": r"[*]+ Path validation summary [*]+\n.*?(?=\n[*]+|$)",
        "modload": r"ModLoad:.*",
        "processor_info": r"=== Processor Information ===\n.*?(?===|$)",
        "system_info": r"=== System Information ===\n.*?(?===|$)",
        "register_states": r"=== Register States ===\n.*?(?===|$)",
        "disassemble_code_32": r"=== Disassemble Code \(EIP\) for 32-bit ===\n.*?(?===|$)",
        "disassemble_code_64": r"=== Disassemble Code \(RIP\) for 64-bit ===\n.*?(?===|$)",
        "memory_info": r"=== Memory Information ===\n.*?(?===|$)",
        "usage_summary": r"--- Usage Summary ----------------.*?(?=---|$)",
        "type_summary": r"--- Type Summary \(for busy\) ------.*?(?=---|$)",
        "state_summary": r"--- State Summary ----------------.*?(?=---|$)",
        "protect_summary": r"--- Protect Summary \(for commit\) -.*?(?=---|$)",
        "largest_region": r"--- Largest Region by Usage -----------.*?(?=---|$)",
        "virtual_memory_layout": r"=== Virtual Memory Layout ===\n.*?(?===|$)",
        "loaded_modules": r"=== Loaded Modules ===\n.*?(?===|$)",
        "dump_memory_contents_32": r"=== Dump Memory Contents \(EIP\) for 32-bit ===\n.*?(?===|$)",
        "dump_memory_contents_64": r"=== Dump Memory Contents \(RIP\) for 64-bit ===\n.*?(?===|$)",
        "list_threads": r"=== List Threads ===\n.*?(?===|$)",
        "thread_info": r"=== Thread Information ===\n.*?(?===|$)",
        "stack_traces": r"=== Stack Traces ===\n.*?(?===|$)",
        "heap_summary": r"=== Heap Summary ===\n.*?(?===|$)",
        "kernel_structures": r"=== Kernel Structures ===\n.*?(?===|$)",
        "kernel_memory_info": r"=== Kernel Memory Information ===\n.*?(?===|$)",
        "kernel_debugging_structures": r"=== Kernel Debugging Structures ===\n.*?(?===|$)",
        "loaded_drivers": r"=== Loaded Drivers ===\n.*?(?===|$)",
        "loaded_images": r"=== Loaded Images ===\n.*?(?===|$)",
        "paged_pools": r"=== Loaded Paged Pools ===\n.*?(?===|$)",
        "kernel_modules": r"=== Kernel Modules ===\n.*?(?===|$)",
        "object_info": r"=== Object Information ===\n.*?(?===|$)",
        "handle_table": r"=== Handle Table ===\n.*?(?===|$)",
        "page_table_entries": r"=== Page Table Entries ===\n.*?(?===|$)",
    }

    # Extract sections based on patterns
    extracted_sections = {}
    for key, pattern in patterns.items():
        matches = re.findall(pattern, content, re.DOTALL)
        if matches:
            extracted_sections[key] = matches

    # Create output directory if it doesn't exist
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # Save extracted sections to separate files with labels
    for section_type, sections in extracted_sections.items():
        for i, section in enumerate(sections):
            section_file_path = os.path.join(output_dir, f"{section_type}_{i+1}.txt")
            with open(section_file_path, 'w') as section_file:
                section_file.write(section)

def process_windbg_outputs(root_dir, output_base_dir):
    windbg_outputs_dir = os.path.join(root_dir, 'windbg_outputs')

    # Iterate through all process folders
    for process_folder in os.listdir(windbg_outputs_dir):
        process_folder_path = os.path.join(windbg_outputs_dir, process_folder)

        # Check if it is a directory
        if os.path.isdir(process_folder_path):
            input_file_path = os.path.join(process_folder_path, 'windbg_output_clipboard.txt')

            # Check if the windbg_output_clipboard.txt file exists
            if os.path.isfile(input_file_path):
                output_dir = os.path.join(output_base_dir, process_folder)
                extract_and_save_sections(input_file_path, output_dir)

if __name__ == "__main__":
    # Set the root directory (the directory containing the 'classifier' folder)
    root_dir = os.path.dirname(os.path.abspath(__file__))

    # Set the output base directory
    output_base_dir = os.path.join(root_dir, 'classifier')

    process_windbg_outputs(root_dir, output_base_dir)
