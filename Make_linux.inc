CC = mpicc
CXX = mpic++
MPIOPT = -oversubscribe

CXXFLAGS+=$(mpic++ --showme:compile)
LIBS+=$(mpic++ --showme:link)
