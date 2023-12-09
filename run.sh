#!/bin/sh

# Execute under build file
for i in $(seq 20 5 50); do
    # Format the outer loop counter i with leading zeros
    i_formatted=$(printf "%02d" "$i")

    # Create a folder with the format "Vertex_$i"
    folder="Vertex__$i_formatted"
    mkdir -p "$folder" || exit

    # Change into the created folder
    cd "$folder" || exit

    # Inner loop to execute graphGen and create files
    for j in $(seq 1 10); do
        # Format the inner loop counter j with leading zeros
        j_formatted=$(printf "%02d" "$j")
        output_filename="Vertex_${i_formatted}_test$j_formatted.txt"
        /home/agurfink/ece650/graphGen/graphGen "$i" > "$output_filename"
        echo "Created file: $output_filename in $folder"
        
        for k in $(seq 1 10); do
            # Execute ece650-prj with content from the inner loop j as input
            result=$(../ece650-prj < "$output_filename")
            
            # Append the result with a newline to the file
            echo "\n$result\n" >> "$output_filename"
        done
    done

    # Go back to the original directory
    cd - > /dev/null 2>&1 || exit
done

