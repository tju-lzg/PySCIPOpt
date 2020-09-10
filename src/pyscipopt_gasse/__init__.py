__version__ = '3.0.2'

# required for Python 3.8 on Windows
import os
if hasattr(os, 'add_dll_directory'):
    os.add_dll_directory(os.path.join(os.getenv('SCIPOPTDIR'), 'bin'))

# export user-relevant objects:
from pyscipopt_gasse.Multidict import multidict
from pyscipopt_gasse.scip      import Model
from pyscipopt_gasse.scip      import Benders
from pyscipopt_gasse.scip      import Benderscut
from pyscipopt_gasse.scip      import Branchrule
from pyscipopt_gasse.scip      import Nodesel
from pyscipopt_gasse.scip      import Conshdlr
from pyscipopt_gasse.scip      import Eventhdlr
from pyscipopt_gasse.scip      import Heur
from pyscipopt_gasse.scip      import Presol
from pyscipopt_gasse.scip      import Pricer
from pyscipopt_gasse.scip      import Prop
from pyscipopt_gasse.scip      import Sepa
from pyscipopt_gasse.scip      import LP
from pyscipopt_gasse.scip      import Expr
from pyscipopt_gasse.scip      import quicksum
from pyscipopt_gasse.scip      import quickprod
from pyscipopt_gasse.scip      import exp
from pyscipopt_gasse.scip      import log
from pyscipopt_gasse.scip      import sqrt
from pyscipopt_gasse.scip      import PY_SCIP_RESULT       as SCIP_RESULT
from pyscipopt_gasse.scip      import PY_SCIP_PARAMSETTING as SCIP_PARAMSETTING
from pyscipopt_gasse.scip      import PY_SCIP_PARAMEMPHASIS as SCIP_PARAMEMPHASIS
from pyscipopt_gasse.scip      import PY_SCIP_STATUS       as SCIP_STATUS
from pyscipopt_gasse.scip      import PY_SCIP_STAGE        as SCIP_STAGE
from pyscipopt_gasse.scip      import PY_SCIP_PROPTIMING   as SCIP_PROPTIMING
from pyscipopt_gasse.scip      import PY_SCIP_PRESOLTIMING as SCIP_PRESOLTIMING
from pyscipopt_gasse.scip      import PY_SCIP_HEURTIMING   as SCIP_HEURTIMING
from pyscipopt_gasse.scip      import PY_SCIP_EVENTTYPE    as SCIP_EVENTTYPE
from pyscipopt_gasse.scip      import PY_SCIP_LPSOLSTAT    as SCIP_LPSOLSTAT
from pyscipopt_gasse.scip      import PY_SCIP_BRANCHDIR    as SCIP_BRANCHDIR
from pyscipopt_gasse.scip      import PY_SCIP_BENDERSENFOTYPE as SCIP_BENDERSENFOTYPE
from pyscipopt_gasse.scip      import PY_SCIP_ROWORIGINTYPE as SCIP_ROWORIGINTYPE
