# Get the classification of all the cuter test problems
import csv
cuter_file = csv.reader(open('cuter_problems.csv', 'rb'), delimiter=',', quotechar='|')
  
# Type of the problem objective function. Possible values:
      #N no objective function is defined,
      #C the objective function is constant,
      #L the objective function is linear,
      #Q the objective function is quadratic,
      #S the objective function is a sum of squares, and
      #O the objective function is none of the above.
allowed_objective = ['N','C','L','Q','S','O']

# Type of constraints. Possible values:
      #U the problem is unconstrained,
      #X the problem's only constraints are fixed variables,
      #B the problem's only constraints are bounds on the variables,
      #N the problem's constraints represent the adjacency matrix of a (linear) network,
      #L the problem's constraints are linear,
      #Q the problem's constraints are quadratic, and
      #O the problem's constraints are more general than any of the above alone.
allowed_constraints = ['O']

# Smoothness of the problem. Possible values:
      #R the problem is regular, that is its first and second derivatives exist and are continuous everywhere, or
      #I the problem is irregular.
allowed_smoothness = 'R'

# Filter out the problems that we are interested in:
problems = []
for (name,classification) in cuter_file:
  # If it matches our criteria
  if classification[0] in allowed_objective and classification[1] in allowed_constraints and classification[2] in allowed_smoothness:
    problems.append(name)

# Now copy the corresponding problems to a new directory
import shutil
for p in problems:
  try:

    # Filename
    filename = p.lower() + ".mod"
    
    # Open the source file
    source=open('cuter_source/'+filename, "r" )
    
    # Create the modified file
    destination= open('cuter_selected/'+filename, "w" )

    # Comment out remaining lines
    comment_out_rest = False

    # Copy line by line
    for line in source:
      # Look for solve command
      if not comment_out_rest and line.startswith("solve"):
        comment_out_rest = True
      
      # Comment out if necessary
      if comment_out_rest:
        destination.write("#")
      
      # Write the (rest of the) line
      destination.write(line)
      
    # Specify filename to save to
    destination.write("\nwrite g" + p.lower() + ";\n")
    
    # Close the files
    source.close()
    destination.close()
  
    print "Successfully copied " + filename
  except IOError:
    print "Failed to copy " + filename

#print problems


